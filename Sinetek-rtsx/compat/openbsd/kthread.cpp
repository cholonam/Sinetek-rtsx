#include "kthread.h"

#include <sys/errno.h> // E*
#include <sys/types.h> // struct proc
#include <kern/thread.h> // kernel_thread_start

#include <AvailabilityMacros.h> // MAC_OS_X_VERSION_MIN_REQUIRED

#define UTL_THIS_CLASS ""
#include "util.h"

struct MyArgStruct {
	// OpenBSD parameters
	void (*func)(void *);
	void *arg;
};

static void my_thread_continue(void *arg, wait_result_t wait_result)
{
	struct MyArgStruct *myArg = (struct MyArgStruct *) arg;

	UTL_CHK_PTR(myArg,);
	auto func = myArg->func;
	auto func_arg = myArg->arg;
	// we can now free myArg (no longer needed)
	UTL_FREE(myArg, struct MyArgStruct);

	UTL_DEBUG_DEF("Thread created (wait_result=%d), calling OpenBSD function...", (int) wait_result);
	// Call OpenBSD thread
	func(func_arg);
	UTL_LOG("OpenBSD thread function returned!");
	// call kthread_exit() ourselves
	kthread_exit(0);
}

int
kthread_create(void (*func)(void *), void *arg, struct proc **newpp, const char *name)
{
	int ret;

	UTL_DEBUG_DEF("Creating new thread (name=%s)...", name);
	auto myArg = UTL_MALLOC(struct MyArgStruct);
	UTL_CHK_PTR(myArg, ENOMEM);

	myArg->func = func;
	myArg->arg = arg;

	thread_t new_thread;
	ret = kernel_thread_start(my_thread_continue, myArg, &new_thread);
	if (ret != KERN_SUCCESS) {
		UTL_ERR("Error %d creating thread!", ret);
		UTL_FREE(myArg, struct MyArgStruct);
		return ret;
	}
#if defined(MAC_OS_X_VERSION_10_15) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
	// set thread name (only Catalina and higher)
	thread_set_thread_name(myArg->thread, name);
#endif
	if (newpp)
		*newpp = (proc *) new_thread;
	thread_deallocate(new_thread);
	return 0;
}

void
kthread_create_deferred(void (*func)(void *), void *arg)
{
	// just assume we can create the thread any time...
	func(arg);
}

void __attribute__((noreturn))
kthread_exit(int ecode)
{
	UTL_DEBUG_FUN("Calling thread_terminate() (should not return)");
	thread_terminate(current_thread());
	UTL_ERR("thread_terminate() returned! Looping...");
	while(1) {}
}
