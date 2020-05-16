#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_RWLOCK_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_RWLOCK_H

#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS

#include "openbsd_compat_types.h" // struct rwlock

__BEGIN_DECLS

void rw_init(struct rwlock *rwl, const char *name);

int rw_assert_wrlock(struct rwlock *rwl);

void rw_enter_write(struct rwlock *rwl);

void rw_exit(struct rwlock *rwl);

__END_DECLS

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_RWLOCK_H
