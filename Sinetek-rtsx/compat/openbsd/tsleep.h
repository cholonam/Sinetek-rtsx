#ifndef SINETEK_RTSX_COMPAT_OPENBSD_TSLEEP_H
#define SINETEK_RTSX_COMPAT_OPENBSD_TSLEEP_H

#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS

#define INFSLP 0xffffffffffffffffull
#define NS_PER_SEC 1000000000ull

#define SEC_TO_NSEC(secs) (NS_PER_SEC * secs)

#define tsleep_nsec Sinetek_rtsx_openbsd_compat_tsleep_nsec
#define wakeup      Sinetek_rtsx_openbsd_compat_wakeup

typedef long long unsigned uint64_t; // required type

__BEGIN_DECLS

int tsleep_nsec(void *ident, int priority, const char *wmesg, uint64_t nsecs);

int wakeup(void *ident);

__END_DECLS

#endif // SINETEK_RTSX_COMPAT_OPENBSD_TSLEEP_H
