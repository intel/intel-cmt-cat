#ifndef __TEST_CAP_H
#define __TEST_CAP_H

#ifndef LOCKFILE
#ifdef __linux__
#define LOCKFILE "/var/lock/libpqos"
#endif
#ifdef __FreeBSD__
#define LOCKFILE "/var/tmp/libpqos.lockfile"
#endif
#endif /*!LOCKFILE*/

#define LOCKFILENO 9999

#endif
