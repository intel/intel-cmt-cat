/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2026 Intel Corporation. All rights reserved.
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
 *
 */

#include "common.h"

#include "log.h"
#include "pqos.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

/* Maximum required file descriptors per core */
#define MAX_FD_PER_CORE 5
/* pqos tool opens some file descriptors while using msr interface */
#define MAX_PQOS_FD 100

/**
 * The permissions a created file is given: 0666, which is what fopen() asks
 * for, narrowed by the umask exactly as it is there. Octal rather than the six
 * S_I* macros, which is the form the rest of this tree uses - lock.c spells the
 * same value LOCKFILE_PERMS 0666 - and the one checkpatch asks for.
 */
#define FOPEN_CREATE_PERMS 0666

/** Room for a mode string with the exclusive modifier taken out of it */
#define FOPEN_MODE_SIZE 8

/**
 * @brief Translate a mode string of fopen() into flags for open()
 *
 * Every mode C defines is translated, and the position of each character
 * matters. The first one carries the access, then '+' and 'b' in either order,
 * and last of all the exclusive 'x' that C11 adds for a 'w' - which makes the
 * exclusive modes exactly "wx", "wbx", "w+x", "w+bx" and "wb+x". Nothing else
 * is a mode C defines, so "wxb" and "wx+" are refused along with the glibc
 * extensions 'e', 'm' and 'c'; a 'b' or an 'x' accepted wherever it appeared
 * would turn "bw" into a translation of "w", and the file would be opened and
 * truncated before fdopen() refused the mode.
 *
 * 'x' asks for the file to be created and to fail if it is already there,
 * which is what O_EXCL does, and takes O_TRUNC out of the flags because a file
 * that cannot exist has nothing to truncate. It is also the one modifier with
 * no meaning for a descriptor that is already open, and glibc's fdopen()
 * refuses "w+x" outright, so it is left out of the mode the stream is made
 * with.
 *
 * This is the only copy of the grammar. The utility had one of its own while
 * its safe_fopen() existed; that wrapper is gone and pqos_fopen() is what the
 * tool calls, so a change here is a change everywhere.
 *
 * @param [in] mode the mode string as pqos_fopen() received it
 * @param [out] flags the flags to open() the file with
 * @param [out] stream_mode the mode fdopen() is given, the same string without
 *              an 'x'; at least FOPEN_MODE_SIZE bytes
 *
 * @return Operation status
 * @retval 0 the mode was translated
 * @retval -1 the mode is not one C defines
 */
static int
fopen_mode_flags(const char *mode, int *flags, char *stream_mode)
{
        int plus = 0;
        int binary = 0;
        int exclusive = 0;
        size_t used = 0;
        size_t i;

        /* refused before the walk below, which starts past the access character
         * and would read past the end of an empty mode
         */
        if (mode[0] == '\0')
                return -1;

        stream_mode[used++] = mode[0];

        for (i = 1; mode[i] != '\0'; i++) {
                if (mode[i] == '+' && !plus)
                        plus = 1;
                else if (mode[i] == 'b' && !binary)
                        binary = 1;
                else if (mode[i] == 'x' && !exclusive && mode[0] == 'w' &&
                         mode[i + 1] == '\0')
                        exclusive = 1;
                else
                        return -1;

                if (mode[i] == 'x')
                        continue;

                if (used >= FOPEN_MODE_SIZE - 1)
                        return -1;
                stream_mode[used++] = mode[i];
        }
        stream_mode[used] = '\0';

        switch (mode[0]) {
        case 'r':
                *flags = plus ? O_RDWR : O_RDONLY;
                break;
        case 'w':
                *flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT;
                *flags |= exclusive ? O_EXCL : O_TRUNC;
                break;
        case 'a':
                *flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
                break;
        default:
                return -1;
        }

        return 0;
}

/**
 * @brief Tell whether fopen() would leave a stream opened with this mode at the
 *        end of the file
 *
 * The C libraries this is built for do not agree: FreeBSD seeks whenever
 * O_APPEND was asked for, "a+" included; glibc positions only a stream that
 * cannot be read, so "a" ends up at the end of the file and "a+" at the start;
 * musl positions none of them. A library this was not built against is taken to
 * behave as musl does and leave the offset alone. The utility had a copy of
 * this beside its own safe_fopen(); both are gone, and this is the only one
 * left.
 *
 * __GLIBC__ comes from <features.h> rather than the compiler, and any glibc
 * header pulls that in, so the test below only means anything underneath one;
 * <stdio.h> is included at the top of this file.
 *
 * @param [in] mode the mode string as pqos_fopen() received it
 *
 * @return Whether the stream belongs at the end of the file
 * @retval 1 fopen() would have positioned it there
 * @retval 0 fopen() would have left it where the descriptor is
 */
static int
fopen_positions_at_end(const char *mode)
{
        if (mode[0] != 'a')
                return 0;

#ifdef __FreeBSD__
        return 1;
#elif defined(__GLIBC__)
        return strchr(mode, '+') == NULL;
#else
        return 0;
#endif
}

/**
 * @brief The length of a name with its trailing slashes left out
 *
 * A trailing slash cannot be carried into the open. The kernel resolves the
 * last component of such a name as a directory, which stops it being the final
 * component: lstat("link/") describes the target and open("link/", O_NOFOLLOW)
 * follows the link, both measured. Checking the leaf and then opening the name
 * as written would only move the window: the check would be advisory, and a
 * link put there in between would still be followed.
 *
 * So the slashes are taken off and the leaf itself is opened, with O_DIRECTORY
 * added: that is what the slash asked for - the name must be a directory - and
 * O_NOFOLLOW then covers the leaf, which is the whole point. The refusal and
 * the open see the same component, so there is no window between them.
 *
 * @param [in] pathname the name as the caller gave it
 * @param [out] len the length without the trailing slashes
 *
 * @return Whether the name has trailing slashes to take off
 * @retval 1 it does, and *len is what is left
 * @retval 0 it does not, or it is nothing but slashes - the root, which is a
 *           directory that no symbolic link can be
 */
static int
path_without_trailing_slashes(const char *pathname, size_t *len)
{
        size_t used = strlen(pathname);

        if (used == 0 || pathname[used - 1] != '/')
                return 0;

        while (used > 0 && pathname[used - 1] == '/')
                used--;

        if (used == 0)
                return 0;

        *len = used;

        return 1;
}

static FILE *
fopen_leaf(const char *name, const char *mode, int extra_flags)
{
        int fd;
        int flags = 0;
        int error;
        char stream_mode[FOPEN_MODE_SIZE] = {0};
        FILE *stream;

        if (name == NULL || mode == NULL ||
            fopen_mode_flags(mode, &flags, stream_mode) != 0) {
                errno = EINVAL;

                return NULL;
        }

        flags |= extra_flags;

        /*
         * The kernel refuses a symbolic link here, before anything is opened,
         * created or truncated. What this replaced asked lstat() whether the
         * name was a link and then handed the name to fopen(), which resolved
         * it a second time: a link planted in between was followed, and a
         * writing mode - which lib/resctrl_monitoring.c uses - truncated the
         * file it pointed at before the comparison that came afterwards could
         * refuse the stream. No race was needed for a link already sitting at
         * the name.
         */
        fd = open(name, flags | O_NOFOLLOW | O_CLOEXEC, FOPEN_CREATE_PERMS);
        if (fd == -1) {
                struct stat lstat_val;

                /*
                 * What open() reports for a link it refused depends on the
                 * platform and on the flags - ELOOP on Linux, EMLINK on
                 * FreeBSD, EEXIST for the O_CREAT | O_EXCL an 'x' mode asks
                 * for - so the name is asked about instead, after the failure,
                 * where the answer only explains it. A link then arrives as
                 * ELOOP wherever this runs, which is what pqos_open() promises
                 * and what a caller printing strerror() reads out to a user.
                 *
                 * errno is put back around that, because lstat() and the
                 * logging are both allowed to change it and the caller is
                 * entitled to the reason the call failed.
                 */
                error = errno;
                if (lstat(name, &lstat_val) == 0 &&
                    S_ISLNK(lstat_val.st_mode)) {
                        LOG_ERROR_IF_INIT("File %s is a symlink\n", name);
                        error = ELOOP;
                }
                errno = error;

                return NULL;
        }

        stream = fdopen(fd, stream_mode);
        if (stream == NULL) {
                error = errno;
                close(fd);
                errno = error;

                return NULL;
        }

        /*
         * fdopen() takes the offset the descriptor has, which open() leaves at
         * the start of the file, while fopen() may put an append stream at the
         * end of it. Writes land at the end either way - that is what O_APPEND
         * is for - but a caller that asks ftell() before writing gets what the
         * fopen() of this platform would have given it.
         */
        if (fopen_positions_at_end(mode)) {
                error = errno;
                (void)fseek(stream, 0, SEEK_END);
                errno = error;
        }

        return stream;
}

int
pqos_fclose(FILE *stream)
{
        return fclose(stream);
}

static int
open_leaf(const char *pathname, int flags, mode_t mode)
{
        int fd;
        int error;
        struct stat lstat_val;
        struct stat fstat_val;
        int new_file = 0;
        int missing = 0;
        int truncate_it = 0;

        if (pathname == NULL) {
                errno = EINVAL;

                return -1;
        }

        /* collect any link info about the file */
        /* coverity[fs_check_call] */
        if (lstat(pathname, &lstat_val) == -1) {
                /**
                 * A caller that asked for the file to be created is entitled to
                 * a name that does not exist yet - "pqos --log-file=<new path>"
                 * names a log the tool is meant to open for the first time. Any
                 * other reason for lstat() to fail ends the call here; a name
                 * that is simply not there is answered below, after the flags
                 * have been judged, since a request this cannot honour is worth
                 * saying so about whether or not the file exists.
                 */
                if (errno != ENOENT)
                        return -1;
                missing = 1;
        } else if (S_ISLNK(lstat_val.st_mode)) {
                /**
                 * Refused here rather than left to the O_NOFOLLOW below,
                 * because what open() reports for a symlink depends on the
                 * flags it is given - O_CREAT | O_EXCL answers EEXIST, O_PATH
                 * opens the link itself, O_DIRECTORY answers ENOTDIR - while
                 * the caller is promised ELOOP.
                 */
                LOG_ERROR_IF_INIT("File %s is a symlink\n", pathname);
                errno = ELOOP;

                return -1;
        }

        /**
         * O_TRUNC is applied after the checks below rather than by the open()
         * itself, because open() applies it to whatever the name refers to at
         * that moment. A name replaced between the lstat() above and the open()
         * is another file, and emptying it is a side effect the comparison
         * further down cannot undo - it could only refuse the descriptor, with
         * the damage done. ftruncate() acts on the file this call has
         * identified and on nothing else.
         *
         * A truncating open has to be writable for that to be possible: POSIX
         * leaves O_RDONLY | O_TRUNC unspecified, ftruncate() refuses a
         * descriptor that cannot write, and turning the request into an open
         * that quietly does not truncate would be worse than refusing it.
         *
         * O_PATH is the exception that is not an error: it carries an access
         * mode and forbids reading or writing through the descriptor, and
         * open() ignores O_TRUNC for it. So the request is honoured the way
         * open() would honour it - the flag is dropped and nothing is truncated
         * - rather than refused for a mode it does not really have.
         *
         * Asked after the name has been looked at, not before: a symbolic link
         * is promised as ELOOP whatever the flags, and a caller that passed a
         * link and a nonsense access mode is told about the link - which is the
         * answer that matters, and the one a caller guarding against a planted
         * link tests for. And asked before the name's existence is answered, so
         * that a request this cannot honour is named as such rather than
         * reported as a missing file.
         */
        if ((flags & O_TRUNC) != 0) {
                int no_io = 0;

#ifdef O_PATH
                no_io = (flags & O_PATH) != 0;
#endif
                if (!no_io && (flags & O_ACCMODE) == O_RDONLY) {
                        errno = EINVAL;

                        return -1;
                }

                flags &= ~O_TRUNC;
                truncate_it = !no_io;
        }

        /**
         * The name is not there, and the caller did not offer to create it.
         */
        if (missing) {
                if ((flags & O_CREAT) == 0) {
                        errno = ENOENT;

                        return -1;
                }
                new_file = 1;
        }

        /**
         * O_NOFOLLOW for the symlink that appears after the lstat() above: it
         * would otherwise be followed here, creating or truncating whatever it
         * points at, and the comparison below could then only refuse the
         * descriptor - with the side effect already done.
         *
         * O_EXCL where the lstat() found nothing, so that the file this then
         * opens is the file it created. Without it, anything that appeared in
         * between - a file somebody else wrote, or a FIFO, which O_NOFOLLOW has
         * no opinion about - would be adopted as though the caller had named
         * it.
         *
         * The mode is passed whatever the flags are. open() reads it only
         * where they create a file, and a flag combination that does - O_CREAT,
         * or the O_TMPFILE that does not carry O_CREAT in its bit pattern -
         * therefore needs no test of its own here.
         */
        fd = open(pathname,
                  flags | O_NOFOLLOW | O_CLOEXEC | (new_file ? O_EXCL : 0),
                  mode);
        if (fd == -1) {
                /**
                 * What a refused symlink is called here depends on the flags
                 * and on the platform - ELOOP on Linux, EMLINK on FreeBSD,
                 * EEXIST under O_EXCL, ENOTDIR under O_DIRECTORY - and none of
                 * them means a symlink on its own either. So the errno is not
                 * read for an answer at all: the name is asked what it is, and
                 * a link is reported as one however the open came back. The
                 * logging can leave an errno of its own behind, so the reason
                 * this returns is put back after the message.
                 */
                error = errno;
                if (lstat(pathname, &lstat_val) == 0 &&
                    S_ISLNK(lstat_val.st_mode)) {
                        LOG_ERROR_IF_INIT("File %s is a symlink\n", pathname);
                        error = ELOOP;
                }
                errno = error;

                return -1;
        }

        /**
         * O_TMPFILE hands back an unnamed inode created inside the directory
         * the name refers to, so the descriptor is not the file the name
         * described: the comparison below would find a regular file where the
         * name is a directory and refuse every such call. The name was still
         * lstat()ed and refused if it was a link, which is the check this
         * function is for.
         */
#ifdef O_TMPFILE
        if ((flags & O_TMPFILE) == O_TMPFILE)
                return fd;
#endif

        /* the file created above is the one to compare against */
        if (new_file && lstat(pathname, &lstat_val) == -1)
                goto pqos_open_error;

        /* collect info about the opened file */
        if (fstat(fd, &fstat_val) == -1)
                goto pqos_open_error;

        /**
         * A descriptor on the symlink itself is what O_PATH | O_NOFOLLOW gives,
         * and a link that appeared after the lstat() above would then be
         * described identically by both stats, so the refusal cannot rest on
         * the flags the caller happened to choose.
         */
        if (S_ISLNK(fstat_val.st_mode)) {
                LOG_ERROR_IF_INIT("File %s is a symlink\n", pathname);
                errno = ELOOP;
                goto pqos_open_error;
        }

        /**
         * A symlink planted at the name after this created the file: the
         * descriptor is the regular file it made, while the name now refers to
         * a link. The identity comparison below would call that "changed",
         * which is true but less than the caller is promised - a name found to
         * be a symbolic link is ELOOP however this arrived at it, and a caller
         * watching for a planted link tests for that.
         */
        if (S_ISLNK(lstat_val.st_mode)) {
                LOG_ERROR_IF_INIT("File %s is a symlink\n", pathname);
                errno = ELOOP;
                goto pqos_open_error;
        }

        /**
         * What is left for the comparison to catch is the name having been
         * replaced by another file of some other kind - not by a link, which is
         * answered above and is not reported as a change. What identifies a
         * file and what kind of file it is are what get compared; a chmod in
         * between changes neither.
         *
         * A directory in the path that is a symlink is invisible to this: the
         * lstat() and the open() resolve it the same way, so they agree about
         * the file at the end of it.
         *
         * And this identifies a file by what the platform can say about it,
         * which is recycled: for a name that already existed, the tuple
         * compared was read before the open, and a name unlinked and created
         * again in that window can carry the inode number it had. So the
         * comparison is best effort, as pqos.h says - what it rests on is that
         * a file recycling the number is still the file the name referred to
         * when open() resolved it, and still not a symbolic link.
         */
        if ((lstat_val.st_mode & S_IFMT) != (fstat_val.st_mode & S_IFMT) ||
            lstat_val.st_ino != fstat_val.st_ino ||
            lstat_val.st_dev != fstat_val.st_dev) {
                LOG_ERROR_IF_INIT("File %s changed while it was being "
                                  "opened\n",
                                  pathname);
                errno = EAGAIN;
                goto pqos_open_error;
        }

        /**
         * The file the checks above identified is the one emptied, once they
         * have all passed - and only where open() itself would have emptied it.
         * O_TRUNC applies to a regular file: open() ignores it for a FIFO, so
         * "pqos -o <fifo>" is a command line that works and has to keep
         * working, and ftruncate() would answer EINVAL for one. A directory or
         * a device never reaches here with a writable open. So the file type
         * this call verified is what decides, which is the same thing open()
         * decides on.
         *
         * A file this call created is left alone: O_EXCL made it, so it was
         * empty, and there is nothing for a truncation to do. Doing it anyway
         * would not be harmless - the name is visible from the moment open()
         * returns, and emptying the file afterwards would erase whatever
         * another process had written to it in between, which the create that
         * open() performs in one step cannot do.
         */
        if (truncate_it && !new_file && S_ISREG(fstat_val.st_mode) &&
            ftruncate(fd, 0) == -1)
                goto pqos_open_error;

        return fd;

pqos_open_error:
        /**
         * The caller reports errno, so the reason the call failed has to
         * survive the close() that tidies up after it.
         */
        error = errno;
        close(fd);
        errno = error;

        return -1;
}

/**
 * @brief Opens the leaf of a stream name written with trailing slashes
 *
 * The slashes are taken off and O_DIRECTORY is added, so the component the
 * caller named is the one opened and O_NOFOLLOW covers it. Where O_DIRECTORY
 * does not exist the promise cannot be kept for this form, and the name is
 * refused rather than served with the promise quietly withdrawn.
 *
 * The descriptor form is a function of its own rather than a branch of this
 * one: a single function told apart by a NULL mode is a function that opens a
 * descriptor when a caller asks for a stream with no mode, and leaks it.
 *
 * @param [in] name the name as the caller gave it
 * @param [in] len its length without the trailing slashes
 * @param [in] mode the mode to open the stream with
 *
 * @return A stream, or NULL with errno set
 */
static FILE *
fopen_a_trailing_slash_name(const char *name, size_t len, const char *mode)
{
#ifdef O_DIRECTORY
        char *leaf = strndup(name, len);
        FILE *stream;
        int error;

        if (leaf == NULL) {
                errno = ENOMEM;

                return NULL;
        }

        stream = fopen_leaf(leaf, mode, O_DIRECTORY);

        error = errno;
        free(leaf);
        errno = error;

        return stream;
#else
        UNUSED_ARG(name);
        UNUSED_ARG(len);
        UNUSED_ARG(mode);

        errno = EINVAL;

        return NULL;
#endif
}

/**
 * @brief Opens the leaf of a name written with trailing slashes
 *
 * The descriptor form of fopen_a_trailing_slash_name(), and the same reasoning.
 *
 * @param [in] pathname the name as the caller gave it
 * @param [in] len its length without the trailing slashes
 * @param [in] flags the flags to open the leaf with
 * @param [in] mode the permissions for a file this creates
 *
 * @return A descriptor, or -1 with errno set
 */
static int
open_a_trailing_slash_name(const char *pathname,
                           size_t len,
                           int flags,
                           mode_t mode)
{
#ifdef O_DIRECTORY
        char *leaf = strndup(pathname, len);
        int error;
        int fd;

        if (leaf == NULL) {
                errno = ENOMEM;

                return -1;
        }

        fd = open_leaf(leaf, flags | O_DIRECTORY, mode);

        error = errno;
        free(leaf);
        errno = error;

        return fd;
#else
        UNUSED_ARG(pathname);
        UNUSED_ARG(len);
        UNUSED_ARG(flags);
        UNUSED_ARG(mode);

        errno = EINVAL;

        return -1;
#endif
}

FILE *
pqos_fopen(const char *name, const char *mode)
{
        size_t len;

        /**
         * A mode this function cannot use is reported before the name is
         * looked at, which is what fopen_leaf() does with it: the name of a
         * directory written with a trailing slash is a name that can be
         * opened, so a call that goes anywhere near opening it before the
         * mode has been judged answers NULL with a descriptor already spent.
         */
        if (name != NULL && mode != NULL &&
            path_without_trailing_slashes(name, &len))
                return fopen_a_trailing_slash_name(name, len, mode);

        return fopen_leaf(name, mode, 0);
}

int
pqos_open(const char *pathname, int flags, mode_t mode)
{
        size_t len;

        if (pathname == NULL) {
                errno = EINVAL;

                return -1;
        }

        if (path_without_trailing_slashes(pathname, &len))
                return open_a_trailing_slash_name(pathname, len, flags, mode);

        return open_leaf(pathname, flags, mode);
}

char *
pqos_strcat(char *dst, const char *src, size_t size)
{
        return strncat(dst, src, size - strnlen(dst, size));
}

char *
pqos_fgets(char *s, int n, FILE *stream)
{
        char *line = NULL;
        size_t line_len = 0;
        ssize_t line_read;
        ssize_t i;

        line_read = getline(&line, &line_len, stream);
        if (line_read != -1) {
                char *p = strchr(line, '\n');

                if (p == NULL)
                        goto pqos_fgets_error;
                *p = '\0';

                if ((ssize_t)strlen(line) != line_read - 1 || line_read > n)
                        goto pqos_fgets_error;
                for (i = 0; i < line_read - 1; ++i)
                        if (!isascii(line[i]))
                                goto pqos_fgets_error;

                strncpy(s, line, n - 1);
                s[n - 1] = '\0';

                free(line);
                return s;
        }

pqos_fgets_error:
        if (line != NULL)
                free(line);

        return NULL;
}

/**
 * @brief Read integer from file
 *
 * @param [in] path file path
 * @param [out] value parsed value
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_fread_uint(const char *path, unsigned *value)
{
        int ret = PQOS_RETVAL_OK;
        FILE *fd;

        fd = pqos_fopen(path, "r");
        if (fd != NULL) {
                if (fscanf(fd, "%u", value) != 1)
                        ret = PQOS_RETVAL_ERROR;
                pqos_fclose(fd);
        } else
                return PQOS_RETVAL_RESOURCE;

        return ret;
}

int
pqos_fread_uint64(const char *fname, unsigned base, uint64_t *value)
{
        FILE *fd;
        char buf[17] = "\0";
        char *s = buf;
        char *endptr = NULL;
        size_t bytes;
        unsigned long long val;

        ASSERT(fname != NULL);
        ASSERT(value != NULL);

        fd = pqos_fopen(fname, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        bytes = fread(buf, sizeof(buf) - 1, 1, fd);
        if (bytes == 0 && !feof(fd)) {
                pqos_fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        pqos_fclose(fd);

        val = strtoull(s, &endptr, base);

        if (!((*s != '\0' && *s != '\n') &&
              (*endptr == '\0' || *endptr == '\n'))) {
                LOG_ERROR("Error converting '%s' to unsigned number!\n", buf);
                return PQOS_RETVAL_ERROR;
        }
        if (val < UINT64_MAX) {
                *value = val;
                return PQOS_RETVAL_OK;
        }

        return PQOS_RETVAL_ERROR;
}

int
pqos_file_exists(const char *path)
{
        return access(path, F_OK) == 0;
}

/**
 * @brief Checks if directory exists
 *
 * @param [in] path file path
 *
 * @return If file exists
 * @retval 1 if file exists
 * @retval 0 if file not exists
 */
int
pqos_dir_exists(const char *path)
{
        struct stat st;

        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int
pqos_file_contains(const char *fname, const char *str, int *found)
{
        FILE *fd;
        char temp[1024];
        int check_symlink = 1;

        if (fname == NULL || str == NULL || found == NULL)
                return PQOS_RETVAL_PARAM;

        if (strncmp(fname, "/proc/", 6) == 0)
                check_symlink = 0;

        if (check_symlink)
                fd = pqos_fopen(fname, "r");
        else
                fd = fopen(fname, "r");

        if (fd == NULL) {
                LOG_DEBUG("%s not found.\n", fname);
                *found = 0;
                return PQOS_RETVAL_OK;
        }

        *found = 0;
        while (fgets(temp, sizeof(temp), fd) != NULL) {
                if (strstr(temp, str) != NULL) {
                        *found = 1;
                        break;
                }
        }

        fclose(fd);
        return PQOS_RETVAL_OK;
}

#define DEV_MEM "/dev/mem"

uint8_t *
pqos_mmap_read(uint64_t address, const uint64_t size)
{
        uint64_t offset;
        uint64_t page_size;
        uint8_t *mem;
        int fd;

        fd = pqos_open(DEV_MEM, O_RDONLY, 0);
        if (fd < 0) {
                LOG_ERROR("Could not open %s\n", DEV_MEM);
                return NULL;
        }

        page_size = sysconf(_SC_PAGESIZE);
        offset = address % page_size;
        mem = mmap(NULL, size + offset, PROT_READ, MAP_PRIVATE, fd,
                   address - offset);

        if (mem == MAP_FAILED) {
                LOG_ERROR("Memory map failed, address=%llx size=%llu\n",
                          (unsigned long long)address,
                          (unsigned long long)size);
                close(fd);
                return NULL;
        }

        close(fd);
        return mem + offset;
}

uint8_t *
pqos_mmap_write(uint64_t address, const uint64_t size)
{
        uint64_t offset;
        uint64_t page_size;
        uint8_t *mem;
        int fd;

        fd = pqos_open(DEV_MEM, O_RDWR, 0);
        if (fd < 0) {
                LOG_ERROR("Could not open %s\n", DEV_MEM);
                return NULL;
        }

        page_size = sysconf(_SC_PAGESIZE);
        offset = address % page_size;
        mem = mmap(NULL, size + offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   address - offset);

        if (mem == MAP_FAILED) {
                LOG_ERROR("Memory map failed, address=%llx size=%llu\n",
                          (unsigned long long)address,
                          (unsigned long long)size);
                close(fd);
                return NULL;
        }

        close(fd);
        return mem + offset;
}

void
pqos_munmap(void *mem, const uint64_t size)
{
        uint64_t offset;
        uint64_t page_size;

        page_size = sysconf(_SC_PAGESIZE);
        offset = (uint64_t)mem % page_size;
        munmap((uint8_t *)mem - offset, size + offset);
}

ssize_t
pqos_read(int fd, void *buf, size_t count)
{
        size_t len = count;
        char *byte_ptr = (char *)buf;
        ssize_t ret;

        if (buf == NULL) {
                errno = EFAULT;
                return -1;
        }

        while (len != 0 && (ret = read(fd, byte_ptr, len)) != 0) {
                if (ret < 0) {
                        if (errno == EINTR)
                                continue;
                        return ret;
                }

                len -= (size_t)ret;
                byte_ptr += ret;
        }

        return count;
}

int
pqos_set_no_files_limit(unsigned long max_core_count)
{
        struct rlimit files_limit;
        const rlim_t required_fd =
            (max_core_count * MAX_FD_PER_CORE) + MAX_PQOS_FD;

        if (getrlimit(RLIMIT_NOFILE, &files_limit))
                return PQOS_RETVAL_ERROR;

        /* Check Kernel allows to open required file descriptors */
        if (files_limit.rlim_max < required_fd ||
            files_limit.rlim_cur < required_fd) {
                if (files_limit.rlim_max < required_fd)
                        files_limit.rlim_max = required_fd;

                files_limit.rlim_cur = required_fd;

                if (setrlimit(RLIMIT_NOFILE, &files_limit))
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}
