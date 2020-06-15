#include "tsleep.h"

#include <sys/errno.h>
#include <IOKit/IOLocks.h> // IORecursiveLock


#include "spl.h" // Sinetek_rtsx_openbsd_compat_spl_getGlobalLock()
#define UTL_THIS_CLASS ""
#include "util.h"

// we need to implement these two in terms of msleep, which is what macOS provides...
// See: https://github.com/apple/darwin-xnu/blob/master/bsd/kern/kern_synch.c

// WARNING: The kernel will crash if msleep is called while a IOSimpleLock is held!

int tsleep_nsec(void *ident, int priority, const char *wmesg, uint64_t nsecs)
{
	UTL_DEBUG_LOOP("%s: tsleep_nsec called (havelock=%d)", wmesg ? wmesg : "(null)",
		      IORecursiveLockHaveLock((IORecursiveLock *) Sinetek_rtsx_openbsd_compat_spl_getGlobalLock()));
	if (!IORecursiveLockHaveLock((IORecursiveLock *) Sinetek_rtsx_openbsd_compat_spl_getGlobalLock())) {
		UTL_DEBUG_LOOP("Lock is not held (wmesg=%s, nsecs=%lld)!", wmesg ? wmesg : "(null)", (int64_t) nsecs);
		IOSleep(1); // just sleep for a bit since we are probably waiting for something
		return EAGAIN;
	}
	int ret;
	if (nsecs == INFSLP) {
		// sleep without deadline
		ret = IORecursiveLockSleep((IORecursiveLock *) Sinetek_rtsx_openbsd_compat_spl_getGlobalLock(),
					   ident, THREAD_UNINT);
	} else {
#if DEBUG
		// in debug mode, increase all timeouts to make sure we give time for logging delays
		if (nsecs * 10 > nsecs) // make sure we don't overflow
			nsecs *= 10;
#endif
		ret = IORecursiveLockSleepDeadline((IORecursiveLock *) Sinetek_rtsx_openbsd_compat_spl_getGlobalLock(),
						   ident, nsecs2AbsoluteTimeDeadline(nsecs), THREAD_UNINT);
	}
	UTL_DEBUG_DEF("tsleep_nsec ret = %d (%s)", ret,
		      ret == THREAD_AWAKENED ? "THREAD_AWAKENED" :
		      ret == THREAD_TIMED_OUT ? "THREAD_TIME_OUT" : "?");
	// See: https://github.com/apple/darwin-xnu/blob/master/bsd/kern/kern_synch.c
	if (ret == THREAD_TIMED_OUT) {
		return EWOULDBLOCK;
	} else {
		return 0;
	}
}

int wakeup(void *ident) {
	UTL_DEBUG_LOOP("wakeup called (havelock=%d)",
		      IORecursiveLockHaveLock((IORecursiveLock *) Sinetek_rtsx_openbsd_compat_spl_getGlobalLock()));
	IORecursiveLockWakeup((IORecursiveLock *) Sinetek_rtsx_openbsd_compat_spl_getGlobalLock(),
			      ident, true);
	UTL_DEBUG_LOOP("wakeup returns");
	return 0;
}
