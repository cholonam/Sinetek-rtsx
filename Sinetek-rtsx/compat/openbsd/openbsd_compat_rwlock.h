#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_RWLOCK_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_RWLOCK_H

#include "openbsd_compat_types.h" // struct rwlock

void rw_init(struct rwlock *rwl, const char *name);

int rw_assert_wrlock(struct rwlock *rwl);

void rw_enter_write(struct rwlock *rwl);

void rw_exit(struct rwlock *rwl);

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_RWLOCK_H
