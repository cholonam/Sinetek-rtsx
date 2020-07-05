#ifndef SINETEK_RTSX_UTIL_H
#define SINETEK_RTSX_UTIL_H

#include <os/log.h>

#include <IOKit/IOLib.h> // IOSleep
#if __cplusplus
#include <IOKit/IOMemoryDescriptor.h> // IOMemoryDescriptor
#endif

#include "util_chk.h"
#include "util_logging.h"
#if __cplusplus
#include "util_dict.h"
#endif
#pragma mark -
#pragma mark Memory functions

#if RTSX_USE_IOMALLOC
#define UTL_MALLOC(TYPE) (TYPE *) UTLMalloc(#TYPE, sizeof(TYPE))
static inline void *UTLMalloc(const char *type, size_t sz) {
	UTL_DEBUG_MEM("Allocating a %s (%u bytes).", type, (unsigned) sz);
	return IOMalloc(sz);
}
#define UTL_FREE(ptr, TYPE) \
do { \
	UTL_DEBUG_MEM("Freeing a %s (%u bytes).", #TYPE, (unsigned) sizeof(TYPE)); \
	IOFree(ptr, sizeof(TYPE)); \
} while (0)
#else // RTSX_USE_IOMALLOC
#define UTL_MALLOC(TYPE) new TYPE
#define UTL_FREE(ptr, TYPE) \
do { \
	if (ptr) { \
		delete (ptr); \
		(ptr) = nullptr; \
	} else { \
		UTL_ERR("Tried to free null pointer (%s) of type %s", #ptr, #TYPE); \
	} \
} while (0)
#endif // RTSX_USE_IOMALLOC

#if DEBUG
#define UTL_SAFE_RELEASE_NULL(ptr) \
do { \
	if (ptr) { \
		(ptr)->release(); \
		(ptr) = nullptr; \
	} else { \
		UTL_ERR("%s: Tried to release null pointer!", #ptr); \
	} \
} while (0)
#define UTL_SAFE_RELEASE_NULL_CHK(ptr, retCnt) \
do { \
	if (ptr) { \
		if ((ptr)->getRetainCount() != retCnt) \
			UTL_ERR("%s: Wrong retain count (%d, expected %d)", #ptr, (ptr)->getRetainCount(), (retCnt)); \
		(ptr)->release(); \
		(ptr) = nullptr; \
	} else { \
		UTL_ERR("%s: Tried to release null pointer!", #ptr); \
	} \
} while (0)
#else
#define UTL_SAFE_RELEASE_NULL(ptr)             OSSafeReleaseNULL(ptr)
#define UTL_SAFE_RELEASE_NULL_CHK(ptr, retCnt) OSSafeReleaseNULL(ptr)
#endif

#if __cplusplus

/// Only valid between prepare() and complete()
static inline size_t bufferNSegments(IOMemoryDescriptor *md) {
	size_t ret = 0;
#if DEBUG
	uint64_t len = md->getLength();

	uint64_t thisOffset = 0;
	while (thisOffset < len) {
		ret++;
		IOByteCount thisSegLen;
		if (!md->getPhysicalSegment(thisOffset, &thisSegLen, kIOMemoryMapperNone)) return 0;
		thisOffset += thisSegLen;
	}
#endif // DEBUG
	return ret;
}

/// Only valid between prepare() and complete()
static inline void dumpBuffer(IOMemoryDescriptor *md) {
#if DEBUG
	auto len = md->getLength();

	uint64_t thisOffset = 0;
	while (thisOffset < len) {
		uint64_t thisSegLen;
		auto addr = md->getPhysicalSegment(thisOffset, &thisSegLen, kIOMemoryMapperNone | md->getDirection());
		UTL_DEBUG_DEF(" - Segment: Addr: 0x%016llx Len: %llu", addr, thisSegLen);
		if (!addr) return;
		thisOffset += thisSegLen;
	}
#endif // DEBUG
}

#endif // __cplusplus

#pragma mark -
#pragma mark Time functions

static inline AbsoluteTime nsecs2AbsoluteTimeDeadline(uint64_t nsecs) {
	AbsoluteTime absInterval, deadline;
	nanoseconds_to_absolutetime(nsecs, &absInterval);
	clock_absolutetime_interval_to_deadline(absInterval, &deadline);
	return deadline;
}

#pragma mark -
#pragma mark Other utility macros

#define RTSX_PTR_FMT "0x%08x%08x"
#define RTSX_PTR_FMT_VAR(ptr) (uint32_t) ((uintptr_t) ptr >> 32), (uint32_t) (uintptr_t) ptr

#define xUTL_STRINGIZE(str) #str
#define UTL_STRINGIZE(str) xUTL_STRINGIZE(str)

#endif /* SINETEK_RTSX_UTIL_H */
