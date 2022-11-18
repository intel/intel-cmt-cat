#ifndef __TEST_CAP_H
#define __TEST_CAP_H

#include <aio.h>
#include <stddef.h>

#ifndef LOCKFILE
#ifdef __linux__
#define LOCKFILE "/var/lock/libpqos"
#endif
#ifdef __FreeBSD__
#define LOCKFILE "/var/tmp/libpqos.lockfile"
#endif
#endif /*!LOCKFILE*/

#define LOCKFILENO 9999

void *__real_malloc(size_t size);
char *__real_getenv(const char *name);
int __real_open(const char *path, int oflags, int mode);
int __real_close(int fildes);
int __real_lockf(int fd, int cmd, off_t len);

#endif
