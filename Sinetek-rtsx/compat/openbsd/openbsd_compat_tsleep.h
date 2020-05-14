#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_TSLEEP_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_TSLEEP_H

#define INFSLP 0xffffffffffffffffull
#define NS_PER_SEC 1000000000ull

#define SEC_TO_NSEC(secs) (NS_PER_SEC * secs)

#define tsleep_nsec Sinetek_rtsx_openbsd_compat_tsleep_nsec
#define wakeup      Sinetek_rtsx_openbsd_compat_wakeup

typedef long long unsigned uint64_t; // required type

int tsleep_nsec(void *ident, int priority, const char *wmesg, uint64_t nsecs);

int wakeup(void *ident);

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_TSLEEP_H
