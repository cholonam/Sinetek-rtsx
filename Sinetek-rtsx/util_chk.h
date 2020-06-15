#ifndef SINETEK_RTSX_UTIL_CHK_H
#define SINETEK_RTSX_UTIL_CHK_H

#include <IOKit/IOReturn.h> // kIOReturnSuccess

#include "util_logging.h" // UTL_ERR

#define UTL_CHK_PTR(ptr, ret) do { \
	if (!(ptr)) { \
		UTL_ERR("null pointer (%s) found!!!", #ptr); \
		return ret; \
	} \
} while (0)

#define UTL_CHK_SUCCESS(expr) \
({ \
	int sinetek_rtsx_utl_chk_success = (expr); \
	if (sinetek_rtsx_utl_chk_success != kIOReturnSuccess) { \
		UTL_ERR("%s returns error 0x%x (%d)", #expr, sinetek_rtsx_utl_chk_success, \
			sinetek_rtsx_utl_chk_success); \
	} \
	sinetek_rtsx_utl_chk_success; \
})

#endif // SINETEK_RTSX_UTIL_CHK_H
