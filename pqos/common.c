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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int
pqos_platform_mem_regions(unsigned *num_mem_regions)
{
        if (pqos_get_num_mem_regions(num_mem_regions) != PQOS_RETVAL_OK) {
                fprintf(stderr, "Memory region information is not "
                                "available!\n");
                return -1;
        }

        return 0;
}

/**
 * @brief Converts a decimal string into an unsigned number.
 *
 * Base 10, so a number a caller read out of a name means the same thing
 * whether or not it was written with a leading zero.
 *
 * @param [in] str string to be converted into unsigned number
 * @param [out] value Numeric value of the string representing the number
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
pqos_parse_uint(const char *str, unsigned *value)
{
        unsigned long val;
        char *endptr = NULL;

        ASSERT(str != NULL);
        ASSERT(value != NULL);

        errno = 0;
        val = strtoul(str, &endptr, 10);
        if (!(*str != '\0' && (*endptr == '\0' || *endptr == '\n')))
                return PQOS_RETVAL_ERROR;

        if (errno == 0 && val <= UINT_MAX) {
                *value = val;
                return PQOS_RETVAL_OK;
        }

        return PQOS_RETVAL_ERROR;
}

__attribute__((noreturn)) void
parse_error(const char *arg, const char *note)
{
        printf("Error parsing \"%s\" command line argument. %s\n",
               arg ? arg : "<null>", note ? note : "");
        exit(EXIT_FAILURE);
}

int
pqos_parse_uint64(const char *text, uint64_t *value)
{
        const char *digits = text;
        char *endptr = NULL;
        uint64_t parsed;
        int base = 10;

        if (text == NULL || value == NULL)
                return -1;

        while (isspace((unsigned char)*digits))
                digits++;
        if (*digits == '\0' || *digits == '-' || *digits == '+')
                return -1;

        if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
                base = 16;
                digits += 2;
        }

        errno = 0;
        parsed = strtoull(digits, &endptr, base);
        while (isspace((unsigned char)*endptr))
                endptr++;
        if (errno != 0 || endptr == digits || *endptr != '\0')
                return -1;

        *value = parsed;
        return 0;
}

int
pqos_parse_mem_regions(const char *arg,
                       int *regions,
                       const unsigned max_regions)
{
        char *copy;
        char *next;
        char *token;
        unsigned count = 0;
        unsigned i;

        if (arg == NULL || regions == NULL || max_regions == 0) {
                fprintf(stderr,
                        "Memory region parser received invalid parameters\n");
                return -1;
        }

        for (i = 0; i < max_regions; i++)
                regions[i] = -1;

        copy = strdup(arg);
        if (copy == NULL) {
                fprintf(stderr,
                        "Failed to allocate memory for region list '%s'\n",
                        arg);
                return -1;
        }

        next = copy;
        for (token = strsep(&next, ","); token != NULL;
             token = strsep(&next, ",")) {
                uint64_t value;
                uint64_t first;
                uint64_t last;
                char *dash;

                /* a token is a single region or a first-last range, the forms
                 * the allocation and the domain options of the same command
                 * lines already take. A leading dash is not a range, so it is
                 * left to the single value path to reject.
                 */
                dash = strchr(token, '-');
                if (dash != NULL && dash != token) {
                        *dash = '\0';
                        if (pqos_parse_uint64(token, &first) != 0 ||
                            pqos_parse_uint64(dash + 1, &last) != 0) {
                                fprintf(stderr,
                                        "Invalid memory region range '%s-%s' "
                                        "in '%s'\n",
                                        token, dash + 1, arg);
                                free(copy);
                                return -1;
                        }
                        if (last < first) {
                                fprintf(stderr,
                                        "Memory region range '%" PRIu64
                                        "-%" PRIu64 "' ends before it starts\n",
                                        first, last);
                                free(copy);
                                return -1;
                        }
                } else {
                        if (pqos_parse_uint64(token, &first) != 0) {
                                fprintf(stderr,
                                        "Invalid memory region '%s' in '%s'\n",
                                        token, arg);
                                free(copy);
                                return -1;
                        }
                        last = first;
                }

                for (value = first; value <= last; value++) {
                        if (count >= max_regions) {
                                fprintf(stderr,
                                        "Too many memory regions in '%s'; "
                                        "maximum is %u\n",
                                        arg, max_regions);
                                free(copy);
                                return -1;
                        }

                        if (value >= PQOS_MAX_MEM_REGIONS) {
                                fprintf(stderr,
                                        "Memory region %" PRIu64
                                        " is out of range [0, %u]\n",
                                        value, PQOS_MAX_MEM_REGIONS - 1);
                                free(copy);
                                return -1;
                        }

                        for (i = 0; i < count; i++)
                                if ((uint64_t)regions[i] == value) {
                                        fprintf(stderr,
                                                "Memory region %" PRIu64
                                                " is selected more than "
                                                "once\n",
                                                value);
                                        free(copy);
                                        return -1;
                                }

                        regions[count++] = (int)value;
                }
        }

        free(copy);
        if (count == 0) {
                fprintf(stderr, "No memory regions specified in '%s'\n", arg);
                return -1;
        }

        return (int)count;
}

#define PCI_ID_MAX_LEN 31

static int
parse_pci_field(const char *text,
                const char *name,
                const unsigned long long max,
                unsigned *value)
{
        unsigned long long parsed;
        char *endptr = NULL;

        if (text == NULL || *text == '\0' || !isxdigit((unsigned char)*text)) {
                fprintf(stderr, "Missing or invalid PCI %s '%s'\n", name,
                        text != NULL ? text : "<null>");
                return -1;
        }

        errno = 0;
        parsed = strtoull(text, &endptr, 16);
        if (errno != 0 || *endptr != '\0' || parsed > max) {
                fprintf(stderr,
                        "PCI %s '%s' is invalid; expected hexadecimal range "
                        "0x0-0x%llx\n",
                        name, text, max);
                return -1;
        }

        *value = (unsigned)parsed;
        return 0;
}

int
pqos_parse_pci_id(char *arg,
                  const int allow_vc,
                  uint16_t *segment,
                  uint16_t *bdf,
                  char **vc)
{
        char id[PCI_ID_MAX_LEN + 1];
        char *first_colon;
        char *second_colon;
        char *dot;
        char *bus_text;
        char *devfn_text;
        char *at;
        size_t id_len;
        unsigned segment_value = 0;
        unsigned bus;
        unsigned device;
        unsigned function;

        if (arg == NULL || segment == NULL || bdf == NULL || vc == NULL) {
                fprintf(stderr, "PCI ID parser received invalid parameters\n");
                return -1;
        }

        *vc = NULL;
        at = strchr(arg, '@');
        if (at != NULL) {
                if (!allow_vc) {
                        fprintf(stderr,
                                "PCI ID '%s' must not include a virtual "
                                "channel\n",
                                arg);
                        return -1;
                }
                if (at[1] == '\0' || strchr(at + 1, '@') != NULL) {
                        fprintf(stderr,
                                "PCI ID '%s' has an invalid virtual channel "
                                "suffix\n",
                                arg);
                        return -1;
                }
                *vc = at + 1;
                id_len = (size_t)(at - arg);
        } else
                id_len = strlen(arg);

        if (id_len == 0 || id_len > PCI_ID_MAX_LEN) {
                fprintf(stderr, "PCI ID '%s' has an invalid length\n", arg);
                return -1;
        }

        memcpy(id, arg, id_len);
        id[id_len] = '\0';

        first_colon = strchr(id, ':');
        if (first_colon == NULL) {
                fprintf(stderr,
                        "PCI ID '%s' must use [segment:]bus:device.function\n",
                        arg);
                return -1;
        }

        second_colon = strchr(first_colon + 1, ':');
        if (second_colon != NULL && strchr(second_colon + 1, ':') != NULL) {
                fprintf(stderr, "PCI ID '%s' has too many separators\n", arg);
                return -1;
        }

        *first_colon = '\0';
        if (second_colon != NULL) {
                *second_colon = '\0';
                if (parse_pci_field(id, "segment", UINT16_MAX,
                                    &segment_value) != 0)
                        return -1;
                bus_text = first_colon + 1;
                devfn_text = second_colon + 1;
        } else {
                bus_text = id;
                devfn_text = first_colon + 1;
        }

        dot = strchr(devfn_text, '.');
        if (dot == NULL || strchr(dot + 1, '.') != NULL) {
                fprintf(stderr,
                        "PCI ID '%s' must use [segment:]bus:device.function\n",
                        arg);
                return -1;
        }
        *dot = '\0';

        if (parse_pci_field(bus_text, "bus", UINT8_MAX, &bus) != 0 ||
            parse_pci_field(devfn_text, "device", 0x1f, &device) != 0 ||
            parse_pci_field(dot + 1, "function", 0x7, &function) != 0)
                return -1;

        *segment = (uint16_t)segment_value;
        *bdf = (uint16_t)((bus << 8) | (device << 3) | function);

        return 0;
}

/**
 * The permissions a created file is given, which is what fopen() gives one and
 * is narrowed by the umask exactly as it is there, so a file this makes is the
 * file it has always made.
 */
#define FOPEN_CREATE_PERMS                                                     \
        (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

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
 * truncated before fdopen() refused the mode, which is the very thing this
 * function exists to prevent.
 *
 * 'x' asks for the file to be created and to fail if it is already there, which
 * is what O_EXCL does, and it takes O_TRUNC out of the flags because a file
 * that cannot exist has nothing to truncate. It is also the one modifier that
 * has no meaning for a descriptor that is already open, and glibc's fdopen()
 * refuses "w+x" outright, so it is left out of the mode the stream is made with
 * - the exclusion has already happened by then.
 *
 * @param [in] mode the mode string as safe_fopen() received it
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
 * There is no one answer to that, and not even a first character of 'a' settles
 * it. All three of these were measured rather than assumed:
 *
 * - FreeBSD seeks whenever the file was opened with O_APPEND, "a+" included
 *   (lib/libc/stdio/fopen.c), and its fdopen() seeks nothing;
 * - glibc positions only a stream that cannot be read, so "a" ends up at the
 *   end of the file and "a+" at the start of it;
 * - musl positions none of them, so every append mode starts where the
 *   descriptor is, which is where fdopen() would have left it anyway.
 *
 * A library this was not built against is taken to behave as musl does, leaving
 * the offset alone - what this wrapper did before it seeked at all, so an
 * unknown library is no worse off than it was. The unit case compares this with
 * the fopen() on the machine rather than with a number, so it is what would
 * report a library that positions a stream some other way.
 *
 * __GLIBC__ is not a compiler macro: <features.h> defines it and any glibc
 * header pulls that in, so the test below only means anything underneath one.
 * <stdio.h> arrives with common.h at the top of this file, which is what makes
 * it safe here - a mode that seeks nothing is what an include order that hid
 * the macro would fall back to.
 *
 * @param [in] mode the mode string as safe_fopen() received it
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

FILE *
safe_fopen(const char *name, const char *mode)
{
        int fd;
        int flags = 0;
        int error;
        char stream_mode[FOPEN_MODE_SIZE] = {0};
        FILE *stream;

        if (name == NULL || mode == NULL ||
            fopen_mode_flags(mode, &flags, stream_mode) != 0) {
                /* the caller asked for something this cannot do, and is told
                 * which kind of failure it was rather than left to guess
                 */
                errno = EINVAL;

                return NULL;
        }

        /*
         * The kernel refuses a symbolic link here, before anything is opened,
         * created or truncated. What this replaced asked lstat() whether the
         * name was a link and then handed the name to fopen(), which resolved
         * it a second time: a link already sitting at the name was followed and
         * the file it pointed at was truncated by a "w" or "w+" mode, and the
         * comparison that came afterwards could only refuse the stream, not put
         * the file back. No race was needed for that, only a link.
         *
         * O_CLOEXEC because the stream outlives the call - the monitoring
         * output file is held for the whole run - and nothing that inherits it
         * has a use for it.
         */
        fd = open(name, flags | O_NOFOLLOW | O_CLOEXEC, FOPEN_CREATE_PERMS);
        if (fd == -1) {
                struct stat lstat_val;

                /*
                 * What open() reports for a link it refused is not one thing:
                 * ELOOP on Linux, EMLINK on FreeBSD, EEXIST under O_EXCL, and
                 * ENOENT for a link whose target is not there, which is what
                 * "w+" on a dangling link answers. So the name is asked about
                 * instead of the errno - after the failure, where the answer
                 * only explains it and cannot affect what was opened - and the
                 * caller is told the same thing safe_open() tells it.
                 *
                 * errno is put back around that, because lstat() and printf()
                 * are both allowed to change it and the caller is entitled to
                 * the reason the call failed.
                 */
                error = errno;
                if (lstat(name, &lstat_val) == 0 &&
                    S_ISLNK(lstat_val.st_mode)) {
                        printf("File %s is a symlink\n", name);
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
         * the start of the file, while fopen() puts an append stream at the end
         * of it. Writes land at the end either way - that is what O_APPEND is
         * for - but a caller that asks ftell() before writing gets what the
         * fopen() of this platform would have given it, which for "a+" is not
         * the same answer everywhere. A target that cannot seek keeps the
         * offset it has, its refusal being an answer rather than a problem, and
         * errno is put back so that a stream returned successfully does not
         * carry one.
         */
        if (fopen_positions_at_end(mode)) {
                error = errno;
                (void)fseek(stream, 0, SEEK_END);
                errno = error;
        }

        return stream;
}

int
safe_open(const char *pathname, int flags, mode_t mode)
{
        int fd;
        int error;
        struct stat lstat_val;
        struct stat fstat_val;
        int new_file = 0;

        /* collect any link info about the file */
        /* coverity[fs_check_call] */
        if (lstat(pathname, &lstat_val) == -1) {
                /**
                 * A caller that asked for the file to be created is entitled
                 * to a name that does not exist yet. Any other reason for
                 * lstat() to fail, and any missing file the caller did not
                 * offer to create, still ends the call.
                 */
                if (errno != ENOENT || (flags & O_CREAT) == 0)
                        return -1;
                new_file = 1;
        } else if (S_ISLNK(lstat_val.st_mode)) {
                /**
                 * Refused here rather than left to the O_NOFOLLOW below,
                 * because what open() reports for a symlink depends on the
                 * flags it is given - O_CREAT | O_EXCL answers EEXIST, O_PATH
                 * opens the link itself - while the caller is promised ELOOP.
                 */
                printf("File %s is a symlink\n", pathname);
                errno = ELOOP;

                return -1;
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
         */
        fd = open(pathname, flags | O_NOFOLLOW | (new_file ? O_EXCL : 0), mode);
        if (fd == -1) {
                /**
                 * What a refused symlink is called here depends on the flags
                 * and on the platform - ELOOP on Linux, EMLINK on FreeBSD,
                 * EEXIST under O_EXCL, ENOTDIR under O_DIRECTORY - and none of
                 * them means a symlink on its own either. So the errno is not
                 * read for an answer at all: the name is asked what it is, and
                 * a link is reported as one however the open came back.
                 * printf() can fail and leave an errno of its own behind, so
                 * the reason this returns is put back after the message.
                 */
                error = errno;
                if (lstat(pathname, &lstat_val) == 0 &&
                    S_ISLNK(lstat_val.st_mode)) {
                        printf("File %s is a symlink\n", pathname);
                        error = ELOOP;
                }
                errno = error;

                return -1;
        }

        /* the file created above is the one to compare against */
        if (new_file && lstat(pathname, &lstat_val) == -1)
                goto safe_open_error;

        /* collect info about the opened file */
        if (fstat(fd, &fstat_val) == -1)
                goto safe_open_error;

        /**
         * A descriptor on the symlink itself is what O_PATH | O_NOFOLLOW
         * gives, and a link that appeared after the lstat() above would then
         * be described identically by both stats, so the refusal cannot rest
         * on the flags the caller happened to choose.
         */
        if (S_ISLNK(fstat_val.st_mode)) {
                printf("File %s is a symlink\n", pathname);
                errno = ELOOP;
                goto safe_open_error;
        }

        /**
         * What is left for the comparison to catch is the name having been
         * replaced between the lstat() and the open() - which is not the same
         * thing as a symlink, and is not reported as one. What identifies a
         * file and what kind of file it is are what get compared; a chmod in
         * between changes neither.
         *
         * A directory in the path that is a symlink is invisible to this: the
         * lstat() and the open() resolve it the same way, so they agree about
         * the file at the end of it.
         */
        if ((lstat_val.st_mode & S_IFMT) != (fstat_val.st_mode & S_IFMT) ||
            lstat_val.st_ino != fstat_val.st_ino ||
            lstat_val.st_dev != fstat_val.st_dev) {
                printf("File %s changed while it was being opened\n", pathname);
                errno = EAGAIN;
                goto safe_open_error;
        }

        return fd;

safe_open_error:
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
 * @brief Reads the CPU number out of a directory entry name
 *
 * @param [in] name directory entry name, expected to be "cpu" and a number
 * @param [out] cpu number the name carries
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK when the whole name is "cpu" followed by a number
 * @retval PQOS_RETVAL_ERROR otherwise, *cpu untouched
 */
static int
cpu_from_name(const char *name, unsigned *cpu)
{
        const char *digits;
        size_t i;

        /* the prefix decides whether there is anything behind it to look at:
         * scandir() calls the filter for "." and ".." as well
         */
        if (strncmp(name, "cpu", 3) != 0)
                return PQOS_RETVAL_ERROR;

        digits = name + 3;
        if (digits[0] == '\0')
                return PQOS_RETVAL_ERROR;

        /* The kernel names these directories "cpu" and a decimal number, and
         * nothing else. strtoul() alone would also take a sign, a leading
         * space and a trailing newline, so "cpu+7", "cpu 7" and "cpu7\n" would
         * all be a seventh CPU beside cpu7 - more entries than the machine has
         * and two of them comparing equal.
         */
        for (i = 0; digits[i] != '\0'; i++)
                if (!isdigit((unsigned char)digits[i]))
                        return PQOS_RETVAL_ERROR;

        return pqos_parse_uint(digits, cpu);
}

/**
 * @brief Filter directory filenames
 *
 * This function is used by the scandir function to filter directories
 *
 * @param dir dirent structure containing directory info
 *
 * @return if directory entry should be included in scandir() output list
 * @retval 0 means don't include the entry
 * @retval 1 means include the entry
 */
int
pqos_filter_cpu(const struct dirent *dir)
{
        unsigned cpu;

        /* the entries this is asked about are the ones pqos_cpu_sort() has to
         * order, so a name that does not carry a CPU number - "cpu0abc" passes
         * a "cpu[0-9]*" glob - is left out here rather than counted as a CPU
         * and ordered on a number nobody could read out of it.
         */
        return cpu_from_name(dir->d_name, &cpu) == PQOS_RETVAL_OK;
}

int
pqos_cpu_sort(const struct dirent **dir1, const struct dirent **dir2)
{
        unsigned cpu1 = 0;
        unsigned cpu2 = 0;
        const int parsed1 = cpu_from_name((*dir1)->d_name, &cpu1);
        const int parsed2 = cpu_from_name((*dir2)->d_name, &cpu2);

        /* an entry whose name holds no CPU number cannot be placed by one, so
         * it goes after everything that can and is ordered against its own
         * kind by name. Nothing reaches here through pqos_filter_cpu(), which
         * keeps the comparator whole for any other caller.
         */
        if (parsed1 != PQOS_RETVAL_OK || parsed2 != PQOS_RETVAL_OK) {
                if (parsed1 == parsed2)
                        return strcmp((*dir1)->d_name, (*dir2)->d_name);

                return parsed1 == PQOS_RETVAL_OK ? -1 : 1;
        }

        /* subtracting the two would be an unsigned difference converted to
         * int, which is only the answer for values far below INT_MAX
         */
        if (cpu1 < cpu2)
                return -1;
        if (cpu1 > cpu2)
                return 1;

        return 0;
}
