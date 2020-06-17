#ifndef SINETEK_RTSX_UTIL_LOGGING_H
#define SINETEK_RTSX_UTIL_LOGGING_H

/* This file can be included by .cpp and .c files */

#include <os/log.h> // os_log_*()

#pragma mark -
#pragma mark Logging macros

#ifndef UTL_THIS_CLASS
#error UTL_THIS_CLASS must be defined before including this file (i.e.: #define UTL_THIS_CLASS "SDDisk::").
#endif

#ifndef UTL_LOG_DELAY_MS
#define UTL_LOG_DELAY_MS 0 /* no delay */
#endif

#define UTL_ERR(fmt, ...) do { \
	os_log_error(OS_LOG_DEFAULT, "rtsx:\t%14s%-22s: " fmt "\n", \
		UTL_THIS_CLASS, __func__, ##__VA_ARGS__); \
	if (UTL_LOG_DELAY_MS) IOSleep(UTL_LOG_DELAY_MS); /* Wait for log to appear... */ \
} while (0)

#define UTL_LOG(fmt, ...) do { \
	os_log(OS_LOG_DEFAULT, "rtsx:\t%14s%-22s: " fmt "\n", \
		UTL_THIS_CLASS, __func__, ##__VA_ARGS__); \
	if (UTL_LOG_DELAY_MS) IOSleep(UTL_LOG_DELAY_MS); /* Wait for log to appear... */ \
} while (0)

#pragma mark -
#pragma mark Debugging macros/functions

#if DEBUG
#ifndef UTL_DEBUG_LEVEL
#	define UTL_DEBUG_LEVEL 0x01 // only default messages
#endif

#define UTL_DEBUG(lvl, fmt, ...) \
do { \
	if (lvl & UTL_DEBUG_LEVEL) { \
		os_log_debug(OS_LOG_DEFAULT, "rtsx:\t%14s%-22s: " fmt "\n", \
		UTL_THIS_CLASS, __func__, ##__VA_ARGS__); \
		if (UTL_LOG_DELAY_MS) IOSleep(UTL_LOG_DELAY_MS); /* Wait for log to appear... */ \
	} \
} while (0)
#else // DEBUG
#define UTL_DEBUG(lvl, fmt, ...) do { } while (0)
#endif // DEBUG

// Debug levels
#define UTL_DEBUG_LVL_DEF	0x01 // Default debug message
#define UTL_DEBUG_LVL_CMD	0x02 // Commands sent to controller
#define UTL_DEBUG_LVL_MEM	0x04 // Memory allocations/releases
#define UTL_DEBUG_LVL_FUN	0x08 // Function calls/entry/exit
#define UTL_DEBUG_LVL_INT	0x10 // Interrupts received
#define UTL_DEBUG_LVL_LOOP	0x20 // For loops that may get too verbose

#define UTL_DEBUG_DEF(...)  UTL_DEBUG(UTL_DEBUG_LVL_DEF,  "[DEF] " __VA_ARGS__)
#define UTL_DEBUG_CMD(...)  UTL_DEBUG(UTL_DEBUG_LVL_CMD,  "[CMD] " __VA_ARGS__)
#define UTL_DEBUG_MEM(...)  UTL_DEBUG(UTL_DEBUG_LVL_MEM,  "[MEM] " __VA_ARGS__)
#define UTL_DEBUG_FUN(...)  UTL_DEBUG(UTL_DEBUG_LVL_FUN,  "[FUN] " __VA_ARGS__)
#define UTL_DEBUG_INT(...)  UTL_DEBUG(UTL_DEBUG_LVL_INT,  "[INT] " __VA_ARGS__)
#define UTL_DEBUG_LOOP(...) UTL_DEBUG(UTL_DEBUG_LVL_LOOP, "[LOOP] " __VA_ARGS__)

static inline const char *mmcCmd2str(uint16_t mmcCmd) {
	switch (mmcCmd) {

		/* SDIO commands */
		case 5: return "SD_IO_SEND_OP_COND";
		case 52: return "SD_IO_RW_DIRECT";
		case 53: return "SD_IO_RW_EXTENDED";

		case 0: return "MMC_GO_IDLE_STATE";
		case 1: return "MMC_SEND_OP_COND";
		case 2: return "MMC_ALL_SEND_CID";
		case 3: return "*_RELATIVE_ADDR";
		case 6: return "*_SWITCH*/SD_APP_BUS_WIDTH";
		case 7: return "MMC_SELECT_CARD";
		case 8: return "MMC_SEND_EXT_CSD/SD_SEND_IF_COND";
		case 9: return "MMC_SEND_CSD";
		case 12: return "MMC_STOP_TRANSMISSION";
		case 13: return "MMC_SEND_STATUS";
		case 16: return "MMC_SET_BLOCKLEN";
		case 17: return "MMC_READ_BLOCK_SINGLE";
		case 18: return "MMC_READ_BLOCK_MULTIPLE";
		case 23: return "MMC_SET_BLOCK_COUNT";
		case 24: return "MMC_WRITE_BLOCK_SINGLE";
		case 25: return "MMC_WRITE_BLOCK_MULTIPLE";
		case 55: return "MMC_APP_CMD";

		case 41: return "SD_APP_OP_COND";
		case 51: return "SD_APP_SEND_SCR";
		default: return "?";
	}
}

#if DEBUG
typedef uint64_t IOByteCount;
static inline const char *busSpaceReg2str(IOByteCount offset) {
	return offset == 0x00 ? "HCBAR" :
	offset == 0x04 ? "HCBCTLR" :
	offset == 0x08 ? "HDBAR" :
	offset == 0x0c ? "HDBCTLR" :
	offset == 0x10 ? "HAIMR" :
	offset == 0x14 ? "BIPR" :
	offset == 0x18 ? "BIER" : "?";
}
#endif // DEBUG

#endif /* SINETEK_RTSX_UTIL_LOGGING_H */
