#include "openbsd_compat_rwlock.h"

#include <machine/locks.h> // lck_rw_*
#define UTL_THIS_CLASS ""
#include "util.h"

void rw_init(struct rwlock *rwl, const char *name)
{
	static lck_grp_t *lck_grp = nullptr;
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(rwl,);
	if (!lck_grp) { // TODO: race condition here!
		// create a group...
		UTL_DEBUG_FUN("Creating group...");
		lck_grp = lck_grp_alloc_init(name, LCK_GRP_ATTR_NULL);
		UTL_CHK_PTR(lck_grp,);
	}
	lck_rw_init((lck_rw_t *) rwl, lck_grp, LCK_ATTR_NULL); // TODO: use a per-driver lock group
	UTL_DEBUG_FUN("END");
}

int rw_assert_wrlock(struct rwlock *rwl)
{
	UTL_DEBUG_FUN("CALLED");
	return 0;
}

void rw_enter_write(struct rwlock *rwl)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(rwl,);
	lck_rw_lock_exclusive((lck_rw_t *) rwl);
	UTL_DEBUG_FUN("END");
}

void rw_exit(struct rwlock *rwl)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(rwl,);
	lck_rw_unlock_exclusive((lck_rw_t *) rwl);
	UTL_DEBUG_FUN("END");
}
