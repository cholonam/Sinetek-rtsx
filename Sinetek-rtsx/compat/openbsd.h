// This file aims to make code imported from OpenBSD compile under macOS with the minimum changes.

#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_H

// include necessary headers
#include <sys/types.h> // size_t (include this first)
#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS
__BEGIN_DECLS
#include <sys/malloc.h> // _MALLOC, _FREE
#include <sys/queue.h>
#include <sys/systm.h> // MIN, EINVAL, ENOMEM, etc...
__END_DECLS

#include <IOKit/IOLib.h> // IOMalloc / IOFree

// other headers from OpenBSD (they have to be at the end since they use types defined here
#define _KERNEL // needed by some headers

#include "openbsd_compat_types.h" // type definitions (must be included before any other openbsd_compat*)
#include "openbsd_compat_bus_space.h" // bus_space_*
#include "openbsd_compat_config.h" // config_*
#include "openbsd_compat_dma.h" // DMA-related functions
#include "openbsd_compat_kthread.h" // kthread_*
#include "openbsd_compat_queue.h" // SIMPLEQ -> STAILQ
#include "openbsd_compat_rwlock.h" // rw_*
#include "openbsd_compat_spl.h" // spl*
#include "openbsd_compat_tsleep.h" // tsleep_nsec

#ifndef UTL_THIS_CLASS
#define UTL_THIS_CLASS ""
#endif
#include "util.h"

// disable execcisve logging from OpenBSD code
#if OPENBSD_CODE_DEBUG
#define printf(...) UTL_DEBUG_DEF(__VA_ARGS__)
#else
#define printf(...) do {} while (0)
#endif

#ifdef KASSERT
#undef KASSERT
#define KASSERT(expr) \
do { \
	if (!(expr)) { \
		UTL_ERR("Assertion failed: %s", #expr); \
	} \
} while (0)
#endif

// bit manipulation macros
#define SET(t, f)    ((t) |=  (f))
#define ISSET(t, f)  ((t) &   (f))
#define CLR(t, f)    ((t) &= ~(f))

// attributes
#define __packed __attribute__((packed))
#define __aligned(N) __attribute__((aligned(N)))

static const int cold = 1;

#include "device.h"
#include "sdmmc_ioreg.h"
#include "sdmmcchip.h"
#include "sdmmcdevs.h"
#include "sdmmcreg.h"
#include "sdmmcvar.h"
#include "rtsxreg.h"
#include "rtsxvar.h"
//#include "Sinetek_rtsx.hpp"

#define be32toh OSSwapBigToHostInt32
#define betoh32 OSSwapBigToHostInt32
#define htole32 OSSwapHostToLittleInt32
#define htole64 OSSwapHostToLittleInt64
#define nitems(v) (sizeof(v) / sizeof((v)[0]))

#define M_CANFAIL 0 // seems like not defined on macOS
#define M_DEVBUF 0 // seems like not defined on macOS

static inline void *malloc(size_t size, int type, int flags) {
	return _MALLOC(size, type, flags);
}
static inline void free(void *addr, int type, int flags) {
	_FREE(addr, type);
}

static inline void delay(unsigned int microseconds) {
	IODelay(microseconds);
}

#define RTSX_PCI_BAR         0x10
#define RTSX_PCI_BAR_525A    0x14
/* syscl - end of 1.4-1.5 change */

#define PCI_PRODUCT_REALTEK_RTS5209     0x5209          /* RTS5209 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTS5227     0x5227          /* RTS5227 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTS5229     0x5229          /* RTS5229 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTS5249     0x5249          /* RTS5249 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTL8402     0x5286          /* RTL8402 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTL8411B    0x5287          /* RTL8411B PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTL8411     0x5289          /* RTL8411 PCI-E Card Reader */

/*
 * syscl - add extra support for new card reader here
 */
#define PCI_PRODUCT_REALTEK_RTS525A     0x525A          /* RTS525A PCI-E Card Reader (XPS 13/15 Series) */

// shut-up warnings
#pragma clang diagnostic ignored "-Wconditional-uninitialized"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wwritable-strings"

extern void *Sinetek_rtsx_openbsd_compat_owner;

/// Call the start function on the ::start() method of the main kext class
int openbsd_compat_start(void *owner);

/// Call the stop function on the ::stop() method of the main kext class
void openbsd_compat_stop(void);

// forward-declare sdmmc_softc
struct sdmmc_softc;

/// Should attach the block device (SDDisk)
int sdmmc_scsi_attach(struct sdmmc_softc *sc);

/// Should detach the block device (SDDisk)
int sdmmc_scsi_detach(struct sdmmc_softc *sc);

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_H
