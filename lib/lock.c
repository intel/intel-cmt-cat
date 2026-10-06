/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @brief Implementation of PQoS API lock
 *
 * provide functions for safe access to PQoS API - this is required for
 * allocation and monitoring modules which also implement PQoS API
 */

#include "lock.h"

#ifdef UNLOCK_CUSTOM

/*
 * Build-time option to disable PQoS API locking.
 *
 * WARNING:
 * - This removes thread/process safety guarantees.
 * - Only use when the application guarantees non-overlapped access to the
 *   resources to be monitored, e.g. RMID range
 */

int
lock_init(void)
{
        return 0;
}

int
lock_fini(void)
{
        return 0;
}

void
lock_get(void)
{
        /* no-op */
}

void
lock_release(void)
{
        /* no-op */
}

#else /* UNLOCK_CUSTOM */

#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>    /* O_CREAT */
#include <inttypes.h> /* PRIu64 */
#include <limits.h>   /* INT_MAX */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* S_Ixxx */
#include <sys/types.h>
#include <unistd.h> /* usleep() */

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
/* The probe file: the write test below creates one, and so does the publishing
 * of the lock, which writes the record into a file of its own and gives it the
 * lock's name with link().
 *
 * The name has to be one nobody can predict, because the lock directory is
 * world writable on every mainstream Linux - /var/lock is a symlink to
 * /run/lock, mode 1777 - so a fixed name is a name any local user can create
 * first, and an O_EXCL open of it then fails with EEXIST for ever. That was a
 * denial of service against every caller of this library, root included: one
 * "touch /var/lock/myapilock.tmp" by any user and pqos_init() reported "Cannot
 * access lock directory" until root removed the file by hand. A name mkostemp()
 * chooses when the probe runs takes the guess away.
 *
 * And it has to be in the directory LOCKFILE is in, which is not one directory:
 * /var/lock on Linux, /var/tmp on FreeBSD, and whatever -DLOCKFILE= says
 * otherwise. link() cannot cross a filesystem, so a probe file in a fixed
 * directory answers EXDEV for every caller whose lock is on another one, and
 * the write test would be testing a directory the lock is not in. Both come
 * from LOCKFILE itself, through lockfile_dir() below.
 */
#define PROBE_NAME                  "pqos-probe-XXXXXX"
/* Room for the directory LOCKFILE is in, or for a probe file beside it. The
 * directory is at most two characters shorter than LOCKFILE - the name it holds
 * and the separator - and what goes in their place is the probe's name and a
 * terminator, so the two sizes together are always enough.
 */
#define LOCKFILE_PATH_LEN           (sizeof(LOCKFILE) + sizeof(PROBE_NAME))
/* The mode the lock file is published with: readable by everybody, writable by
 * the caller that holds it and by nobody else.
 *
 * Both halves are needed. Every other caller reads this file to find out who
 * holds the lock, and one that cannot read it is told the lock is there and
 * cannot be examined - which fails closed, so the lock can never be cleared
 * again. And a record any local user may write is not a lock at all: the holder
 * is identified by what the file says, so a user who writes "1 1" into a live
 * root-owned lock has it read as a dead holder, after which the next caller
 * removes it and runs beside the session that still holds it.
 */
#define LOCKFILE_PERMS              0644
#define MAX_LINE                    128
/* The third field of a record this library writes. Earlier libraries wrote two
 * numbers and no version, and the second was not the start time - their walk
 * over /proc/<pid>/stat read field 23 - so a record without this field cannot
 * be checked against a start time, and is not read as though it could be.
 */
#define LOCKFILE_RECORD_VERSION     2
/* /proc/<pid>/stat field 22 is the process start time. The walk below starts
 * after field 2 - the process name in parentheses, which may contain spaces and
 * parentheses of its own, so it is found by the *last* ')' on the line and not
 * by counting from the start - and then skips one delimiter per field it
 * passes, 19 of them to arrive at the 22nd from the 3rd.
 *
 * Counting from the start of the line instead landed on a different field for
 * any process whose name holds a space, and skipping one field too many landed
 * on the virtual memory size, which is not a start time and not even constant:
 * it changes as the process allocates, so a live session's own lock file could
 * be judged stale by the next invocation, which would then remove it and take a
 * lock somebody else was holding. Either way the protection against a reused
 * PID did nothing.
 */
#define PROC_STAT_FIELDS_AFTER_COMM 19
#define STAT_PATH_LENGTH            64
#define PROCFILE_BUF_LEN            1024

/**
 * API thread/process safe access is secured through these locks.
 */
static int m_apilock = -1;
static pthread_mutex_t m_apilock_mutex;
static pid_t m_pid = 0;
/**
 * @brief Opens a file as a stream whose descriptor closes on exec
 *
 * fopen() leaves the descriptor open across an exec, so anything this process
 * starts inherits it. What that costs here is not the lock itself - the lock is
 * the file existing and the pid written in it, and these streams neither hold
 * nor release it - but a descriptor handed to a program that has no business
 * with it, on a file this library owns, for as long as that program runs. The
 * descriptor is made with open(), which is where O_CLOEXEC can be asked for,
 * and the stream is put on top of it. The descriptor the lock is taken with is
 * kept in m_apilock and has always been opened this way.
 *
 * @param [in] path file to open
 * @param [in] oflags flags for open(), without O_CLOEXEC
 * @param [in] fmode mode string for fdopen()
 * @param [in] perms permission bits, used where the flags create the file
 *
 * @return A stream, or NULL with errno set
 */
static FILE *
fopen_cloexec(const char *path, int oflags, const char *fmode, mode_t perms)
{
        int fd = open(path, oflags | O_CLOEXEC, perms);
        FILE *fp;

        if (fd == -1)
                return NULL;

        fp = fdopen(fd, fmode);
        if (fp == NULL) {
                int error = errno;

                close(fd);
                errno = error;
        }

        return fp;
}

/* 64 bits, and not unsigned long: /proc/<pid>/stat states field 22 as an
 * unsigned long long, and this tree builds for __i386__ where a long is 32 of
 * them - on which a machine up long enough (about 50 days at 1000 Hz) has a
 * start time that does not fit, every read of it fails with ERANGE, and no
 * caller can take the lock at all.
 */
static uint64_t m_start_time = 0;

/**
 * @brief helper: Get process start time
 *
 * Three answers, for the reason the lock file's own reader has three: "there is
 * no such record" and "the record could not be read" are different things, and
 * one value for both decided the question wrongly in both directions. As this
 * process's own start time, a failure was published as zero - a record naming a
 * start time no process can have, which the next caller reads as a dead holder.
 * As another holder's, a failure was read as "dead" and its lock removed, so a
 * transient EIO, an unreadable /proc, or this process being out of descriptors
 * took a lock off a live session.
 *
 * ENOENT is the first answer and not the second: a process that has gone has no
 * /proc entry, and a platform that publishes none has no /proc entry either -
 * neither is a failure to look.
 *
 * @param [in] pid process id
 * @param [out] start_time the start time, zero where there is no record
 *
 * @return Operation status
 * @retval 1 a start time was read
 * @retval 0 there is no record for \a pid
 * @retval -1 the record could not be read, with errno set
 */
static int
get_process_start_time(pid_t pid, uint64_t *start_time)
{
        char stat_path[STAT_PATH_LENGTH];
        char buf[PROCFILE_BUF_LEN];
        char *p = buf;
        char *end = NULL;
        int i;
        size_t nread;
        uint64_t value = 0;

        *start_time = 0;

        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
        FILE *fp = fopen_cloexec(stat_path, O_RDONLY, "r", 0);

        if (!fp)
                return errno == ENOENT ? 0 : -1;

        /* The whole record and not one line of it. The process name is field 2
         * and may contain a newline - this kernel does not escape one,
         * measured: a process renamed "a\nb" has "(a" and "b)" on separate
         * lines of /proc/<pid>/stat - so a line-wise read returns a prefix with
         * no closing parenthesis in it, the walk below finds nothing, and a
         * live holder is taken for a dead one. Its lock file is then removed
         * and a second session takes a lock the first still believes it holds.
         */
        /* cleared first: a failed link() leaves EEXIST and fopen() does not
         * reset it, so a stream that reports an error without setting one of
         * its own would have the fallback below report "File exists"
         */
        errno = 0;
        nread = fread(buf, 1, sizeof(buf) - 1, fp);

        /* asked of the stream and not of the count: fread() may hand back some
         * bytes and set the error indicator on the same call, and a record cut
         * off part way through field 22 leaves a prefix of it that strtoul()
         * accepts - a start time that is not this process's, published as if it
         * were, or another holder's read as a mismatch and its lock removed
         */
        if (ferror(fp)) {
                int error = errno;

                fclose(fp);
                /* what the read failed with, and not what closing the stream
                 * left behind: the caller prints this. EIO where the stream
                 * reports an error and errno says nothing, because -1 promises
                 * an errno and "Success" is not one
                 */
                errno = error != 0 ? error : EIO;

                return -1;
        }

        fclose(fp);

        /* an entry that is there and says nothing: /proc never does this, so it
         * is this process failing to read rather than a process that has gone.
         *
         * EINVAL here and on the two parse failures below, because -1 promises
         * an errno and these paths have none of their own: the caller prints
         * strerror(errno), and what it printed was whatever the last unrelated
         * call had left there - "Success", most often.
         */
        if (nread == 0) {
                errno = EINVAL;

                return -1;
        }
        buf[nread] = '\0';

        /* past the process name, whatever is in it: the last ')' in the record
         * closes field 2, because the name is the only field that can hold one
         */
        p = strrchr(buf, ')');
        if (!p || *(p + 1) != ' ') {
                errno = EINVAL;

                return -1;
        }
        p += 2;

        for (i = 0; i < PROC_STAT_FIELDS_AFTER_COMM; ++i) {
                p = strchr(p, ' ');
                if (!p) {
                        errno = EINVAL;

                        return -1;
                }
                ++p;
        }

        /* a record that does not parse is the same answer: the entry is there
         * and this did not get a start time out of it. strtoul() rather than a
         * single variable sscanf(), which checkpatch asks about and which
         * cannot tell a field of no digits from one it did not reach
         */
        errno = 0;
        value = strtoull(p, &end, 10);
        if (end == p) {
                errno = EINVAL;

                return -1;
        }
        if (errno != 0)
                return -1;

        *start_time = value;

        return 1;
}

/**
 * @brief Opens what sits at LOCKFILE, where that is the kind of thing a lock is
 *
 * The lock directory is world writable on every mainstream Linux - /var/lock is
 * a symlink to /run/lock, mode 1777 - so anything can be sitting at this name,
 * and
 * what was there a moment ago need not be there now. What this library writes
 * there is always a regular file, created with O_EXCL; a symbolic link, a
 * directory, a fifo or a device node is not a lock anybody is holding, whoever
 * it belongs to.
 *
 * One open answers all of that, and answers it about the file the caller then
 * reads. Asking with lstat() and opening afterwards is two looks at one name in
 * a directory anybody may write: between them the owner can put a fifo there,
 * and a privileged caller blocks in open() for as long as the attacker likes,
 * or a symbolic link, which that open would follow out of the directory.
 * O_NOFOLLOW refuses the link, O_NONBLOCK refuses to wait on the fifo, and
 * fstat() of the descriptor describes the file the descriptor is on and no
 * other.
 *
 * The owner is deliberately not part of the question. A file owned by another
 * user naming a live process is indistinguishable from a legitimate lock held
 * by an unprivileged session - nothing in this library requires root, and the
 * resctrl and perf paths are the ordinary unprivileged case - so a root caller
 * that cleared it because it could not have written it would be taking a lock
 * off a live holder, and two sessions would drive the same counters believing
 * they each held it. That is the accident this file exists to prevent, and it
 * is worse than the denial of service that honouring the file leaves open: one
 * is silent, the other says which PID it is waiting for. Closing that needs the
 * lock directory not to be world writable, which is a packaging question and
 * not one this function can answer.
 *
 * So what decides is liveness, as it always did, and this only opens the lock
 * file to ask about.
 *
 * Three answers and not two, because "I could not look" is not "there is
 * nothing there". An open that fails because the name is a symbolic link, or
 * because nothing is at it any more, has said what it is; one that fails
 * because this process is out of descriptors has said nothing, and treating
 * that as a stale lock would unlink the marker of a session that really is
 * holding it - after which a third process could start beside the holder. So an
 * error that does not describe the file fails closed.
 *
 * @param [out] fd the descriptor, where the return is 1
 *
 * @retval 1 a regular file, open at \a fd and the caller's to close
 * @retval 0 nothing a lock of ours could be, and nothing left open
 * @retval -1 it could not be examined, so nothing may be concluded about it
 */
static int
open_the_lockfile(int *fd)
{
        struct stat st;
        int opened;

        opened = open(LOCKFILE, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        if (opened < 0) {
                /* what the name is: a link O_NOFOLLOW refused, a name that is
                 * gone, a path that stopped being one, or a device nobody may
                 * open. Anything else - out of descriptors, out of memory, a
                 * permission that says nothing about the file's kind - is this
                 * process failing to look
                 */
                switch (errno) {
                case ELOOP:
                        /* the same answer, and the same cause: what open()
                         * reports for a link it refused depends on the platform
                         * - ELOOP on Linux, EMLINK on FreeBSD, as lib/common.c
                         * says where it meets the same thing. Without it a
                         * symbolic link at the lock's name is "could not be
                         * examined" there, which fails closed and leaves any
                         * local user able to stop every caller of this library
                         * with one ln -s.
                         */
#ifdef EMLINK
                case EMLINK:
#endif
                case ENOENT:
                case ENOTDIR:
                case ENXIO:
                        return 0;
                default:
                        return -1;
                }
        }

        if (fstat(opened, &st) != 0) {
                int error = errno;

                close(opened);
                /* why the file could not be examined, and not what closing it
                 * made of that: the caller prints this one
                 */
                errno = error;

                return -1;
        }

        if (!S_ISREG(st.st_mode)) {
                close(opened);

                return 0;
        }

        *fd = opened;

        return 1;
}

/**
 * @brief Writes the directory LOCKFILE is in into \a dir
 *
 * Taken from LOCKFILE rather than named a second time, because LOCKFILE is not
 * one path: /var/lock/libpqos on Linux, /var/tmp/libpqos.lockfile on FreeBSD,
 * and whatever -DLOCKFILE= says otherwise. A second name for the directory is a
 * second name that can disagree with it, and this file has two uses that must
 * not: the write test, which would otherwise test a directory the lock is not
 * in, and the probe file, which link() cannot move across a filesystem.
 *
 * @param [out] dir where to write the directory
 * @param [in] size how much room \a dir has
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -1 if \a dir is too small
 */
static int
lockfile_dir(char *dir, size_t size)
{
        const char *slash = strrchr(LOCKFILE, '/');
        size_t length;

        /* no directory in the name at all: the current one */
        if (slash == NULL) {
                if (size < 2)
                        return -1;
                dir[0] = '.';
                dir[1] = '\0';

                return 0;
        }

        /* and "/libpqos" leaves the root directory, which is one character and
         * not none
         */
        length = (size_t)(slash - LOCKFILE);
        if (length == 0)
                length = 1;

        if (length >= size)
                return -1;

        memcpy(dir, LOCKFILE, length);
        dir[length] = '\0';

        return 0;
}

/**
 * @brief Writes a mkostemp() template for a probe file beside LOCKFILE
 *
 * @param [out] path where to write the template
 * @param [in] size how much room \a path has
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -1 if \a path is too small
 */
static int
lockfile_probe_template(char *path, size_t size)
{
        char dir[LOCKFILE_PATH_LEN];
        const char *separator = "/";
        int written;

        if (lockfile_dir(dir, sizeof(dir)) != 0)
                return -1;

        /* a lock in the root directory leaves "/", and a separator is not added
         * to it twice
         */
        if (dir[0] == '/' && dir[1] == '\0')
                separator = "";

        written = snprintf(path, size, "%s%s%s", dir, separator, PROBE_NAME);
        if (written < 0 || (size_t)written >= size)
                return -1;

        return 0;
}

/**
 * @brief Check if the lockfile's directory is accessible
 *
 * @return Operation status
 * retval 0 on success
 * retval -1 on failure
 */
static int
check_lockdir_access(void)
{
        char dir[LOCKFILE_PATH_LEN];
        char template[LOCKFILE_PATH_LEN];
        int fd;

        if (lockfile_dir(dir, sizeof(dir)) != 0 ||
            lockfile_probe_template(template, sizeof(template)) != 0)
                return -1;

        if (access(dir, R_OK | W_OK | X_OK) != 0)
                return -1;

        /* A file of a name chosen here and now, so that what this reports is
         * whether the directory can be written to - and not whether somebody
         * else has already created the name this would have used. It is made
         * 0600, which is what a file nobody else has any business with should
         * be, and close on exec, which is why mkostemp() and not mkstemp():
         * mkstemp() takes no flags, so a thread that exec()s between the
         * creation and the close would hand the child a descriptor on a file in
         * the lock directory.
         */
        fd = mkostemp(template, O_CLOEXEC);
        if (fd == -1)
                return -1;
        close(fd);
        if (unlink(template) != 0)
                return -1;
        return 0;
}

/**
 * @brief Check if PID is alive and matches start time
 *
 * @param [in] pid process id
 * @param [in] start_time process start time
 *
 * @return Operation status
 * @retval 1 the process is alive and started at \a start_time
 * @retval 0 it is not
 * @retval -1 it could not be determined, with errno set
 */
static int
is_pid_alive(pid_t pid, uint64_t start_time)
{
        uint64_t cur_start_time = 0;
        int got = get_process_start_time(pid, &cur_start_time);

        if (got < 0)
                return -1;

        return got == 1 && cur_start_time == start_time;
}

/**
 * @brief Whether a process of this id is running, whenever it started
 *
 * The answer for a record whose start time cannot be checked, and the one that
 * fails closed: a holder that is running keeps the lock, and a process that
 * took its id since is mistaken for it - after which the lock is left for
 * whoever knows it is stale to remove, which is what the caller's message
 * says.
 *
 * @param [in] pid process id
 *
 * @retval 1 it is running
 * @retval 0 it is not
 * @retval -1 it could not be determined, with errno set
 */
static int
is_pid_running(pid_t pid)
{
        uint64_t start_time = 0;

        return get_process_start_time(pid, &start_time);
}

/**
 * @brief Reads the numbers a lock file record is made of
 *
 * Checked conversions and not scanf's: this file is in a world writable
 * directory, so both numbers are whatever any local user cared to put there,
 * and scanf's integer conversions are undefined where the value does not fit
 * the destination - a privileged caller must not reach undefined behaviour by
 * looking at a forged lock. A sign is refused outright, which also closes the
 * quieter half of that: strtoull() accepts "-1" and hands back UINT64_MAX.
 *
 * Nothing this library writes is refused by any of it, so everything refused
 * here is a record this library did not write - which the caller reads as a
 * lock nobody holds.
 *
 * A record with no version, or with one this library does not write, is read
 * and answered as such: an earlier library's session, or a later one's, may be
 * holding it, and its second number cannot be compared with a start time.
 *
 * @param [in] line the record, NUL terminated
 * @param [out] pid process id
 * @param [out] start_time its start time, or what an unverifiable record says
 *
 * @return Operation status
 * @retval 1 a record of this library's version was read
 * @retval 2 a record of another version was read: \a pid is its holder and
 *         \a start_time cannot be checked
 * @retval 0 the record is not one any version of this library wrote
 */
static int
parse_record(const char *line, pid_t *pid, uint64_t *start_time)
{
        char *end = NULL;
        long long id;
        unsigned long long when;
        unsigned long long version = 0;

        /* a sign is not part of either number, and the first field is a process
         * id: pid_t is a signed int type on every platform this builds for, so
         * INT_MAX is the range, and zero and below name no process
         */
        if (*line == '-' || *line == '+')
                return 0;

        errno = 0;
        id = strtoll(line, &end, 10);
        if (end == line || errno != 0 || id <= 0 || id > (long long)INT_MAX)
                return 0;

        /* one space, as written */
        if (*end != ' ')
                return 0;
        ++end;
        if (*end == '-' || *end == '+')
                return 0;

        line = end;
        errno = 0;
        when = strtoull(line, &end, 10);
        if (end == line || errno != 0)
                return 0;

        /* the version, after one space, where there is one */
        if (*end == ' ' && end[1] >= '0' && end[1] <= '9') {
                line = end + 1;
                errno = 0;
                version = strtoull(line, &end, 10);
                if (errno != 0)
                        return 0;
        }

        /* and nothing after it but the end of the line */
        while (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')
                ++end;
        if (*end != '\0')
                return 0;

        *pid = (pid_t)id;
        *start_time = (uint64_t)when;

        return version == LOCKFILE_RECORD_VERSION ? 1 : 2;
}

/**
 * @brief Read lock file (PID and start time)
 *
 * Three answers and not two, for the reason open_the_lockfile() gives three:
 * "there is no record in it" and "it could not be read" are different things.
 * The first is a stale lock, which the caller clears. The second says nothing
 * about who holds the lock - a read that fails with EIO is this process failing
 * to look, exactly as a failure to open is - and clearing a lock on the
 * strength of it would unlink a live holder's marker.
 *
 * @param [in] fd the lock file, open and vouched for by open_the_lockfile()
 * @param [out] pid process id
 * @param [out] start_time process start time
 *
 * @return Operation status
 * @retval 1 a record was read, and \a pid and \a start_time are it
 * @retval 2 a record of another version was read: \a pid is its holder and
 *         \a start_time cannot be checked
 * @retval 0 the file was read and holds no record
 * @retval -1 the file could not be read, with errno set
 */
static int
read_lockfile(int fd, pid_t *pid, uint64_t *start_time)
{
        char line[MAX_LINE];
        size_t got = 0;

        /* the descriptor open_the_lockfile() checked, and not the name again:
         * what a name resolves to can change between two opens, and the point
         * of that one open was that this reads the file it vouched for
         */
        /* read until the file ends or there is no room left, rather than once:
         * a read may return fewer bytes than it was asked for, and a signal
         * arriving after some have been transferred is one of the ways it does.
         * A record cut in half parses as one field instead of two, and a record
         * that does not parse is a stale lock as far as the caller is concerned
         * - after which it unlinks a live holder's. EINTR is retried for the
         * same reason, and for the reason the write is.
         */
        while (got < sizeof(line) - 1) {
                ssize_t chunk = read(fd, line + got, sizeof(line) - 1 - got);

                if (chunk < 0) {
                        if (errno == EINTR)
                                continue;

                        /* it could not be read, which is not the same as
                         * holding no record: see the three answers below
                         */
                        return -1;
                }

                /* the end of the file: the whole record is in hand, or there
                 * was never a whole record to have
                 */
                if (chunk == 0)
                        break;

                got += (size_t)chunk;
        }

        if (got == 0)
                return 0;
        line[got] = '\0';

        return parse_record(line, pid, start_time);
}

/**
 * @brief Writes a whole buffer, or says it could not
 *
 * write() may write fewer bytes than it was given and may be interrupted before
 * it writes any, and a record half written is a lock file that reads as stale.
 * Neither is likely for a short write to a regular file, and neither is
 * excluded by anything.
 *
 * @param [in] fd what to write to
 * @param [in] buf the bytes
 * @param [in] length how many of them
 *
 * @retval 0 all of them were written
 * @retval -1 they were not, with errno from the call that stopped
 */
static int
write_all(int fd, const char *buf, size_t length)
{
        size_t done = 0;

        while (done < length) {
                ssize_t wrote = write(fd, buf + done, length - done);

                if (wrote < 0) {
                        if (errno == EINTR)
                                continue;

                        return -1;
                }

                if (wrote == 0) {
                        errno = EIO;

                        return -1;
                }

                done += (size_t)wrote;
        }

        return 0;
}

/**
 * @brief Creates the lock file with its record already in it
 *
 * The name appears only once what is behind it is complete. A create followed
 * by a write leaves a window in which the lock file is a regular file with
 * nothing in it - and another caller arriving then reads no record, judges the
 * file stale, removes it and creates its own, after which both believe they
 * hold the lock. The window is microseconds wide and two pqos_init() calls are
 * enough to be in it.
 *
 * So the record is written into a file of its own and that file is given the
 * lock's name with link(), which creates the name or fails and never replaces
 * what is there. EEXIST from it means somebody else got the name first, which
 * is the same answer an exclusive create gave.
 *
 * @param [in] pid the process to record
 * @param [in] start_time its start time
 *
 * @return the descriptor on the published file, or -1 with errno set
 */
static int
create_the_lockfile(pid_t pid, uint64_t start_time)
{
        char template[LOCKFILE_PATH_LEN];
        char record[MAX_LINE];
        int fd;
        int length;
        int error;

        /* beside the lock, because the link() below cannot cross a
         * filesystem
         */
        if (lockfile_probe_template(template, sizeof(template)) != 0)
                return -1;

        fd = mkostemp(template, O_CLOEXEC);
        if (fd < 0)
                return -1;

        length = snprintf(record, sizeof(record), "%d %" PRIu64 " %d\n",
                          (int)pid, start_time, LOCKFILE_RECORD_VERSION);

        /* 0600 is what mkostemp() makes, and the lock file has to be readable
         * by anybody - it is read by every other caller to find out who holds
         * it - so the mode is widened once, to LOCKFILE_PERMS.
         *
         * Set rather than left to the umask, which is what the O_CREAT open
         * this replaced did: a caller whose umask takes the read bits away
         * published a lock no other user could examine, and that is the one
         * failure this file cannot recover from. What the umask must not be
         * able to do is the other direction, and it cannot: the mode is a
         * constant and the write bits are not in it.
         */
        if (length <= 0 || (size_t)length >= sizeof(record) ||
            write_all(fd, record, (size_t)length) != 0 ||
            fchmod(fd, LOCKFILE_PERMS) != 0 || link(template, LOCKFILE) != 0) {
                error = errno;
                close(fd);
                unlink(template);
                errno = error;

                return -1;
        }

        /* the name it was made under goes; the name it was linked to stays, and
         * this descriptor is on the file both referred to
         */
        unlink(template);

        return fd;
}

/**
 * @brief Clears whatever is sitting at the lock's name
 *
 * rmdir() as well as unlink(), because a directory is one of the things that
 * can be sitting at this name in a world writable directory and unlink() cannot
 * remove one: it fails with EISDIR on Linux and EPERM on FreeBSD, and the
 * caller would be told to remove a stale lock file by hand for as long as the
 * directory was there.
 *
 * An empty one. A directory with a child in it gives ENOTEMPTY and the caller
 * is told to clear it by hand, so that shape of the denial of service is still
 * open - deliberately: emptying a tree somebody else controls, in a directory
 * they can keep writing to, is a race against them with deletion as the prize,
 * and no library should take it. What closes it is the lock not living
 * somewhere world writable, which is the same answer as for a lock file naming
 * a live process of their own, and it is packaging rather than code.
 *
 * The same answer covers one more: this removes a name, and what a name
 * resolves to can change after it has been examined - the owner can put a
 * fresh, live lock there in between, and it is that one this unlinks.
 * Validating a descriptor cannot help, because the deletion is of a path;
 * nothing short of a directory hostile writers cannot reach makes the two refer
 * to the same thing.
 *
 * Gone already is success and not failure. The name that reaches here is one
 * link() found taken and nobody was found holding, and its owner may remove it
 * in between - a session finalising normally does exactly that. ENOENT means
 * the name is free, which is what this was asked for, and the create that
 * follows is exclusive, so a lock that appears instead of nothing is still
 * refused.
 *
 * @return Operation status
 * @retval 0 the name is free
 * @retval -1 it is not, with errno from the call that could not clear it
 */
static int
remove_the_lockfile(void)
{
        int error;

        if (unlink(LOCKFILE) == 0 || errno == ENOENT)
                return 0;

        error = errno;
        if (error != EISDIR && error != EPERM)
                return -1;

        if (rmdir(LOCKFILE) == 0 || errno == ENOENT)
                return 0;

        /* and if that says the name is not a directory, the unlink's error is
         * the one to report: an unprivileged caller meeting another user's
         * stale file in a sticky directory gets EPERM from unlink() and ENOTDIR
         * from rmdir(), and being told the path is not a directory sends
         * whoever reads it after the wrong thing. EPERM is why it could not be
         * removed; ENOTDIR is only how this found out that the fallback does
         * not apply.
         */
        if (errno == ENOTDIR)
                errno = error;

        return -1;
}

int
lock_init(void)
{
        pid_t pid;
        uint64_t stime;
        int start_time_known;
        static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

        pthread_mutex_lock(&init_mutex);

        /* Check directory access */
        if (check_lockdir_access() != 0) {
                char dir[LOCKFILE_PATH_LEN];

                /* the directory the lock is in, so that a caller who chose it
                 * with -DLOCKFILE= is told about the one they chose
                 */
                if (lockfile_dir(dir, sizeof(dir)) != 0)
                        dir[0] = '\0';

                fprintf(stderr,
                        "Cannot access lock directory: \"%s\". Run program as "
                        "root!\n",
                        dir);
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }

        /* What the lock file says is known before the lock file exists: the
         * name is created by linking a file that already holds the record, so
         * no caller can see it incomplete.
         *
         * O_CLOEXEC: the descriptor is held for the whole session, and a child
         * that inherited it could release a lock it does not hold.
         */
        m_pid = getpid();
        start_time_known = get_process_start_time(m_pid, &m_start_time);
        if (start_time_known < 0) {
                /* the record this process would be identified by cannot be
                 * read, so the one it would publish names a start time that is
                 * not its own - which the next caller reads as a dead holder
                 * and clears, taking the lock off this session
                 */
                fprintf(stderr,
                        "Cannot read this process's start time: \"%s\".\n",
                        strerror(errno));
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }
#ifdef __linux__
        if (start_time_known == 0) {
                /* and no record at all, on the platform whose liveness check is
                 * that record: every later check would answer the same way, so
                 * a lock published here cannot be told from a stale one and the
                 * next caller removes it and runs beside this session. A lock
                 * that cannot exclude is worse than no lock, so this refuses
                 * instead.
                 *
                 * Linux only, because the check is: a platform that publishes
                 * no such record needs a liveness test of its own before it can
                 * be held to this, and the FreeBSD one is not written yet.
                 */
                fprintf(stderr,
                        "No \"/proc/%d/stat\" to read this process's start "
                        "time from. Is /proc mounted?\n",
                        (int)m_pid);
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }
#endif

        m_apilock = create_the_lockfile(m_pid, m_start_time);
        if (m_apilock == -1) {
                if (errno == EEXIST) {
                        /* Lock file exists: it is either a lock to respect, or
                         * a stale one to clear, or - since anybody may write
                         * in the lock directory - not a lock at all
                         */
                        int lockfd = -1;
                        int held = 0;
                        int unverified = 0;
                        int examined = open_the_lockfile(&lockfd);

                        if (examined == 1) {
                                /* and reading it asks the same question once
                                 * more: a file with no record in it is stale, a
                                 * file that could not be read is unknown, and
                                 * the unknown is handled below as a failure to
                                 * examine it - because that is what it is
                                 */
                                int record =
                                    read_lockfile(lockfd, &pid, &stime);
                                int error = errno;

                                close(lockfd);
                                /* what the read failed with, which is what the
                                 * message below prints - closing the descriptor
                                 * can leave an errno of its own
                                 */
                                errno = error;

                                if (record < 0) {
                                        examined = -1;
                                } else if (record > 0) {
                                        /* and the same three answers about the
                                         * process it names: one that cannot be
                                         * asked about is not one that has gone.
                                         * A record of another version names a
                                         * holder whose start time cannot be
                                         * checked, so it holds while it runs
                                         */
                                        int alive =
                                            record == 1
                                                ? is_pid_alive(pid, stime)
                                                : is_pid_running(pid);

                                        if (alive < 0)
                                                examined = -1;
                                        else
                                                held = alive;
                                        unverified = record == 2;
                                }
                        }

                        if (examined == -1) {
                                /* It is there and could not be examined, so
                                 * whether anybody holds it is unknown - and the
                                 * safe unknown is "somebody does". Clearing it
                                 * on the strength of a failure to look would
                                 * unlink the marker of a live session, and a
                                 * third process could then start beside it.
                                 */
                                fprintf(stderr,
                                        "Lock file \"%s\" is there and could "
                                        "not be examined: \"%s\". Not removing "
                                        "it.\n",
                                        LOCKFILE, strerror(errno));
                                pthread_mutex_unlock(&init_mutex);
                                return -1;
                        }

                        if (!held) {
                                /* File is stale, so clear the name - see
                                 * remove_the_lockfile(), which is where what
                                 * that can and cannot do is written down
                                 */
                                if (remove_the_lockfile() != 0) {
                                        fprintf(
                                            stderr,
                                            "Cannot remove stale lock file: "
                                            "%s. Error: \"%s\". You should "
                                            "remove the stale file manually or "
                                            "run program as root!\n",
                                            LOCKFILE, strerror(errno));
                                        pthread_mutex_unlock(&init_mutex);
                                        return -1;
                                }
                                /* Creating the lock file after removing the
                                 * stale one - the same way as above, so this
                                 * one is published complete too
                                 */
                                m_apilock =
                                    create_the_lockfile(m_pid, m_start_time);
                                if (m_apilock == -1) {
                                        /* EEXIST here means the file came back
                                         * between the unlink above and this
                                         * open - somebody else is creating it
                                         * as fast as it is removed, and looping
                                         * would be a race this cannot win
                                         */
                                        fprintf(stderr,
                                                "Couldn't create lock file: "
                                                "\"%s\". Error: \"%s\".\n",
                                                LOCKFILE, strerror(errno));
                                        pthread_mutex_unlock(&init_mutex);
                                        return -1;
                                }
                        } else {
                                /* Lock file is valid and owned by a living
                                 * process, and the pid is the one already read
                                 * from the descriptor that was checked - not
                                 * the name read a second time, which could by
                                 * then be a different file
                                 */
                                fprintf(stderr,
                                        "Lock file \"%s\" is already in use "
                                        "by PID %d\n",
                                        LOCKFILE, pid);
                                if (unverified)
                                        fprintf(stderr,
                                                "It was written by another "
                                                "version of the library and "
                                                "names a running process. Stop "
                                                "that session, or remove the "
                                                "file if PID %d is not one.\n",
                                                pid);
                                pthread_mutex_unlock(&init_mutex);
                                return -1;
                        }
                } else {
                        fprintf(stderr,
                                "Couldn't create lock file: %s. Error: %s\n",
                                LOCKFILE, strerror(errno));
                        pthread_mutex_unlock(&init_mutex);
                        return -1;
                }
        }

        if (pthread_mutex_init(&m_apilock_mutex, NULL) != 0) {
                close(m_apilock);
                unlink(LOCKFILE);
                m_apilock = -1;
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }

        pthread_mutex_unlock(&init_mutex);
        return 0;
}

int
lock_fini(void)
{
        int ret = 0;

        if (close(m_apilock) != 0)
                ret = -1;

        if (pthread_mutex_destroy(&m_apilock_mutex) != 0)
                ret = -1;

        /* Remove lock file on exit */
        if (unlink(LOCKFILE) != 0)
                ret = -1;

        m_apilock = -1;
        return ret;
}

void
lock_get(void)
{
        if (pthread_mutex_lock(&m_apilock_mutex) != 0)
                fprintf(stderr, "API mutex lock error: %s\n", strerror(errno));
}

void
lock_release(void)
{
        if (pthread_mutex_unlock(&m_apilock_mutex) != 0)
                fprintf(stderr, "API mutex unlock error: %s\n",
                        strerror(errno));
}

#endif /* UNLOCK_CUSTOM */
