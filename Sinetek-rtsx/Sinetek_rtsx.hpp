#pragma once

#include <stdint.h>
#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <IOKit/pci/IOPCIDevice.h>
#pragma clang diagnostic pop
#include <IOKit/IOInterruptEventSource.h>
#if RTSX_USE_IOCOMMANDGATE
#include <IOKit/IOCommandGate.h>
#endif
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

__BEGIN_DECLS
#include "sdmmcvar.h"
__END_DECLS

// forward declarations
struct rtsx_softc;
struct IOInterruptEventSource;

class SDDisk;
struct Sinetek_rtsx : public IOService
{

	OSDeclareDefaultStructors(Sinetek_rtsx);

public:
	// implementing init() causes the kext not to unload!
	virtual bool init(OSDictionary *dictionary = nullptr) override;
	virtual void free() override;

	virtual bool start(IOService * provider) override;
	virtual void stop(IOService * provider) override;

	static void InterruptHandler(OSObject *ih, IOInterruptEventSource *ies, int count);

	void rtsx_pci_attach();
	void rtsx_pci_detach();
	/* syscl - Power Management Support */
	virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService * policyMaker) override;

	void blk_attach();
	void blk_detach();

	uint32_t READ4(IOByteCount offset);
	static bool InterruptFilter(OSObject *arg, IOFilterInterruptEventSource *source);

	// ** //
	IOPCIDevice *		provider_;
	IOWorkLoop *		workloop_;
	IOMemoryMap *		map_;
	IOMemoryDescriptor *	memory_descriptor_;
	IOFilterInterruptEventSource *intr_source_;
#if RTSX_USE_IOCOMMANDGATE
	void executeOneAsCommand();
	static int executeOneCommandGateAction(
					       OSObject *sc,
					       void *newRequest,
					       void *, void *, void * );
#endif

	SDDisk *			sddisk_;

#if RTSX_USE_IOLOCK
	IOLock *		intr_status_lock;
	IORecursiveLock *	splsdmmc_rec_lock;
	bool			intr_status_event;
#endif
	rtsx_softc *		rtsx_softc_original_;

#if RTSX_USE_IOCOMMANDGATE
	IOCommandGate *		task_command_gate_;
#else
	/*
	 * Task thread.
	 */
	IOWorkLoop *		task_loop_;
#endif
	IOTimerEventSource *	task_execute_one_;
	bool			write_enabled_; /// true if write is enabled
	void			task_add();
	void			prepare_task_loop();
	void			destroy_task_loop();
	static void		task_execute_one_impl_(OSObject *, IOTimerEventSource *);

	void cardEject();
	bool cardIsWriteProtected();
	bool writeEnabled();
};
