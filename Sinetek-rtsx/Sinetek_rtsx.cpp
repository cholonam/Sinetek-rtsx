#include "Sinetek_rtsx.hpp"

#include <pexpert/pexpert.h> // PE_parse_boot_argn
#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOTimerEventSource.h>
#if RTSX_USE_IOFIES
#include <IOKit/IOFilterInterruptEventSource.h>
#endif

#undef super
#define super IOService
OSDefineMetaClassAndStructors(Sinetek_rtsx, super);

#include "compat/openbsd/config.h" // config_detach
#include "rtsxreg.h"
#include "rtsxvar.h" // rtsx_softc
#include "SDDisk.hpp"

#undef UTL_THIS_CLASS
#define UTL_THIS_CLASS "Sinetek_rtsx::"
#include "util.h"

// We need these functions, which are not declared in rtsxvar.h
__BEGIN_DECLS
int	rtsx_init(struct rtsx_softc *, int);
void	rtsx_card_eject(struct rtsx_softc *);
__END_DECLS

//
// syscl - define & enumerate power states
//
enum {
	kPowerStateSleep  = 0,
	kPowerStateNormal = 1,
	kPowerStateCount
};

//
// syscl - Define usable power states
//
static IOPMPowerState ourPowerStates[kPowerStateCount] =
{
	// state 0 (sleep)
	{ kIOPMPowerStateVersion1 },
	// state 1 (normal)
	{
		kIOPMPowerStateVersion1,
		kIOPMPowerOn               // device is on and requires (and provides) power
		| kIOPMDeviceUsable        // this is the lowest state at which the device is usable
		| kIOPMInitialDeviceState, // initially the device is at this state (this should prevent spurious call)
		kIOPMPowerOn,
		kIOPMPowerOn
	}
};

// Use a global variable, since it will be accessed from the BSD code
int Sinetek_rtsx_boot_arg_mimic_linux = 0;

bool Sinetek_rtsx::init(OSDictionary *dictionary) {
	if (!super::init()) return false;
	UTL_DEBUG_FUN("START");

	// initialize variables / allocate memory
	if (!(rtsx_softc_original_ = UTL_MALLOC(struct rtsx_softc))) {
		UTL_ERR("ERROR ALLOCATING MEMORY!");
		return false;
	}
	// set device name (activate() will need it)
	// TODO: Calling config_found
	strlcpy(rtsx_softc_original_->sc_dev.dv_xname, "rtsx", sizeof(rtsx_softc_original_->sc_dev.dv_xname));
	uint32_t dummy;
	write_enabled_ = !PE_parse_boot_argn("-rtsx_ro", &dummy, sizeof(dummy));
	if (!write_enabled_) {
		UTL_LOG("Read-only mode");
	}
	Sinetek_rtsx_boot_arg_mimic_linux = (int) PE_parse_boot_argn("-rtsx_mimic_linux", &dummy, sizeof(dummy));
	UTL_DEBUG_FUN("END");
	return true;
}

void Sinetek_rtsx::free() {
	UTL_DEBUG_FUN("START");
	// deallocate memory
	UTL_FREE(rtsx_softc_original_, struct rtsx_softc);

	super::free();
	UTL_DEBUG_FUN("END");
}

bool Sinetek_rtsx::start(IOService *provider)
{
	UTL_DEBUG_FUN("START");
	if (!super::start(provider))
		return false;

#if RTSX_USE_IOLOCK
	this->intr_status_lock = IOLockAlloc();
	this->splsdmmc_rec_lock = IORecursiveLockAlloc();
	this->intr_status_event = false;
#endif // RTSX_USE_IOLOCK
	assert(OSDynamicCast(IOPCIDevice, provider));
	provider_ = OSDynamicCast(IOPCIDevice, provider);
	if (!provider_)
	{
		UTL_ERR("IOPCIDevice cannot be cast.");
		goto ERROR;
	}

	workloop_ = getWorkLoop();
	if (!workloop_) goto ERROR;
	/*
	 * Enable memory response from the card.
	 */
	provider_->setMemoryEnable(true);

	prepare_task_loop();
	if (openbsd_compat_start(this)) {
		goto ERROR;
	}
	rtsx_pci_attach();
	
	PMinit();
	provider_->joinPMtree(this);
	if (registerPowerDriver(this, ourPowerStates, kPowerStateCount) != IOPMNoErr)
	{
		UTL_ERR("registerPowerDriver failed");
	}
	UTL_DEBUG_DEF("Power management initialized");

	UTL_LOG("Driver started (version %s %s build)", UTL_STRINGIZE(MODULE_VERSION),
#if DEBUG
		"debug");
#else
		"release");
#endif
	return true;
ERROR:
	workloop_ = NULL;
#if RTSX_USE_IOLOCK
	if (splsdmmc_rec_lock)
		IORecursiveLockFree(splsdmmc_rec_lock);
	if (intr_status_lock)
		IOLockFree(intr_status_lock);
#endif

	UTL_DEBUG_FUN("END");
	return false;
}

// WATCH OUT: stop() may not be called if start() fails!
void Sinetek_rtsx::stop(IOService *provider)
{
	rtsx_pci_detach();
	openbsd_compat_stop();
	destroy_task_loop();
	UTL_SAFE_RELEASE_NULL(workloop_);
	PMstop();

	IOSleep(1000); // give the worker thread some time to finish TODO: Fix this!

	super::stop(provider);
	UTL_LOG("Driver stopped.");
}

void Sinetek_rtsx::trampoline_intr(OSObject *ih, IOInterruptEventSource *ies, int count)
{
	UTL_DEBUG_INT("Interrupt received (ies=" RTSX_PTR_FMT " count=%d)!", RTSX_PTR_FMT_VAR(ies), count);
	/* go to isr handler */
	auto self = OSDynamicCast(Sinetek_rtsx, ih);
	if (self && self->rtsx_softc_original_)
		::rtsx_intr(self->rtsx_softc_original_);
	else
		UTL_ERR("Object received is not a Sinetek_rtsx!");
}

// From https://developer.apple.com/library/archive/documentation/HardwareDrivers/Conceptual/ThunderboltDevGuide/Basics02/Basics02.html
static int findMSI(IOPCIDevice *provider) {
	int index  = 0;
	int source = 0;
	for (index = 0; ; index++) {
		IOReturn result   = kIOReturnSuccess;
		int interruptType = 0;

		result = provider->getInterruptType (index, &interruptType);
		if (result != kIOReturnSuccess)
			return -1;
		if (interruptType & kIOInterruptTypePCIMessaged) {
			source = index;
			UTL_LOG("MSI interrupt found at index %d", index);
			return index;
		}
	}
}

// This method is called only from start()
void Sinetek_rtsx::rtsx_pci_attach()
{
	uint device_id;
	int bar = RTSX_PCI_BAR;

	UTL_DEBUG_FUN("START");

	if ((provider_->extendedConfigRead16(RTSX_CFG_PCI) & RTSX_CFG_ASIC) != 0)
	{
		printf("no asic\n");
		return;
	}

	/* Enable the device */
	provider_->setBusMasterEnable(true);

	/* Map device memory with register. */
	device_id = provider_->extendedConfigRead16(kIOPCIConfigDeviceID);
	if (device_id == PCI_PRODUCT_REALTEK_RTS525A)
		bar = RTSX_PCI_BAR_525A;
	map_ = provider_->mapDeviceMemoryWithRegister(bar);
	if (!map_) {
		UTL_ERR("Could not get device memory map!");
		return;
	}
	memory_descriptor_ = map_->getMemoryDescriptor();
	if (!memory_descriptor_) {
		UTL_ERR("Could not get device memory descriptor!");
		return;
	}

	/* Map device interrupt. */
	int intIndex = findMSI(provider_);
	if (intIndex < 0)
		intIndex = 0; // no MSI found -> use default index
#if RTSX_USE_IOFIES
	intr_source_ = IOFilterInterruptEventSource::filterInterruptEventSource(this, trampoline_intr, is_my_interrupt,
										provider_, intIndex);
#else // RTSX_USE_IOFIES
	intr_source_ = IOInterruptEventSource::interruptEventSource(this, trampoline_intr, provider_, intIndex);
#endif // RTSX_USE_IOFIES
	if (!intr_source_)
	{
		printf("can't map interrupt source\n");
		return;
	}
	workloop_->addEventSource(intr_source_);
	intr_source_->enable();

	/* Get the vendor and try to match on it. */
	//device_id = provider_->extendedConfigRead16(kIOPCIConfigDeviceID);
	int flags;
	switch (device_id) {
		case PCI_PRODUCT_REALTEK_RTS5209:
			flags = RTSX_F_5209;
			break;
		case PCI_PRODUCT_REALTEK_RTS5229:
		case PCI_PRODUCT_REALTEK_RTS5249:
			flags = RTSX_F_5229;
			break;
		case PCI_PRODUCT_REALTEK_RTS525A:
			/* syscl - RTS525A */
			flags = RTSX_F_525A;
			break;
		default:
			flags = 0;
			break;
	}

	UTL_CHK_PTR(this->rtsx_softc_original_,);
	UTL_DEBUG_DEF("Calling attach%s...", (flags & RTSX_F_525A) ? " (525A detected)" : "");
	int error = ::rtsx_attach(this->rtsx_softc_original_, gBusSpaceTag,
				  (bus_space_handle_t) memory_descriptor_,
				  0/* ignored */,
				  gBusDmaTag, flags);

	if (!error) {
		//		pci_present_and_attached_ = true;
		if (this->rtsx_softc_original_->flags & RTSX_F_SDIO_SUPPORT)
			UTL_DEBUG_DEF("SDIO detected");
	} else {
		UTL_ERR("rtsx_attach returned error %d", error);
	}
	UTL_DEBUG_FUN("END");
}

/// In OpenBSD this is not implemented, but that means that the detach() should be passed on to the children (sdmmc).
/// An SDDisk is created when an SD card is inserted, but the sdmmc is active all the time. For that reason, we call
/// detach for the sdmmc here and not in SDDisk.
void Sinetek_rtsx::rtsx_pci_detach()
{
	// call detach on sdmmc
	// TODO: This should actually be called on rtsx_softc_original_, but the logic of calling children's detach()
	// is not implemented yet at the compatibility layer. So since we know that the only child is sdmmc, we call it
	// directly.
	config_detach(rtsx_softc_original_->sdmmc, 0);
	//sdmmc_detach(rtsx_softc_original_->sdmmc, 0);

	UTL_CHK_PTR(workloop_,);
	UTL_CHK_PTR(intr_source_,);

	workloop_->removeEventSource(intr_source_);
	UTL_SAFE_RELEASE_NULL(intr_source_);
#if RTSX_USE_IOLOCK
	// should this be called in free()?
	UTL_CHK_PTR(splsdmmc_rec_lock,);
	UTL_CHK_PTR(intr_status_lock,);
	IORecursiveLockFree(this->splsdmmc_rec_lock);
	this->splsdmmc_rec_lock = nullptr;
	IOLockFree(this->intr_status_lock);
	this->intr_status_lock = nullptr;
#endif // RTSX_USE_IOLOCK
	UTL_SAFE_RELEASE_NULL(map_);
}

// This method is called from a different thread. To prevent setPowerState() from being called after we call
// registerPowerDriver(), we set the kIOPMInitialDeviceState flag in our initial power state.
// Many services (even logging) may not be available when this method is called.
IOReturn Sinetek_rtsx::setPowerState(unsigned long powerStateOrdinal, IOService *policyMaker)
{
	auto previousState = this->getPowerState();
	UTL_DEBUG_FUN("START (powerState: %u -> %u)", (unsigned) previousState, (unsigned) powerStateOrdinal);

	switch (powerStateOrdinal) {
		case kPowerStateSleep:
			// save state
			rtsx_activate(&rtsx_softc_original_->sc_dev, DVACT_SUSPEND);
			break;
		case kPowerStateNormal:
			// re-initialize chip
			rtsx_init(rtsx_softc_original_, 1);

			// restore state
			rtsx_activate(&rtsx_softc_original_->sc_dev, DVACT_RESUME);
			break;
		default:
			UTL_DEBUG_DEF("Ignoring unknown power state (%lu)", powerStateOrdinal);
			break;
	}
	UTL_DEBUG_FUN("END");
	return IOPMAckImplied;
}

void Sinetek_rtsx::prepare_task_loop()
{
	UTL_CHK_PTR(workloop_,);
#if RTSX_USE_IOCOMMANDGATE
	task_command_gate_ = IOCommandGate::commandGate(this, rtsx_softc::executeOneCommandGateAction);
	workloop_->addEventSource(task_command_gate_);
#endif
#if RTSX_USE_IOCOMMANDGATE
	// connect the timer to the interrupt workloop_
	task_execute_one_ = IOTimerEventSource::timerEventSource(this, task_execute_one_impl_);
	workloop_->addEventSource(task_execute_one_);
#else
	task_loop_ = IOWorkLoop::workLoop();
	task_execute_one_ = IOTimerEventSource::timerEventSource(this, task_execute_one_impl_);
	task_loop_->addEventSource(task_execute_one_);
#endif
}

void Sinetek_rtsx::destroy_task_loop()
{
	UTL_CHK_PTR(task_execute_one_,);
	task_execute_one_->cancelTimeout();
#if RTSX_USE_IOCOMMANDGATE
	UTL_CHK_PTR(workloop_,)
	workloop_->removeEventSource(task_command_gate_);
	UTL_CHK_PTR(task_command_gate_,);
	UTL_SAFE_RELEASE_NULL(task_command_gate_);
	UTL_CHK_PTR(workloop_,);
	workloop_->removeEventSource(task_execute_one_);
	UTL_SAFE_RELEASE_NULL(task_execute_one_);
#else
	UTL_CHK_PTR(task_loop_,);
	task_loop_->removeEventSource(task_execute_one_);
	UTL_SAFE_RELEASE_NULL(task_execute_one_);
	UTL_SAFE_RELEASE_NULL(task_loop_);
#endif
}

void Sinetek_rtsx::task_execute_one_impl_(OSObject *target, IOTimerEventSource *sender)
{
	extern void read_task_impl_(void *_args);
	UTL_DEBUG_DEF("  ===> TOOK TASK WORKLOOP...");
	if (!target) return;
	Sinetek_rtsx *sc = OSDynamicCast(Sinetek_rtsx, target);
	struct sdmmc_task *task;

#if RTSX_USE_IOLOCK
	IORecursiveLockLock(sc->splsdmmc_rec_lock);
#endif
	struct sdmmc_softc *sdmmc = (struct sdmmc_softc *) sc->rtsx_softc_original_->sdmmc;
	auto tskq = &sdmmc->sc_tskq;
	for (task = TAILQ_FIRST(tskq); task != NULL;
	     task = TAILQ_FIRST(tskq)) {
		UTL_DEBUG_LOOP("  => Executing one task (CRASHED HERE!)...");
		// remove task from queue (clang static analyzer gives false "Use of memory after it is freed")
		sdmmc_del_task(task);
		UTL_DEBUG_LOOP("  => Task deleted from queue");
		task->func(task->arg);
		UTL_DEBUG_LOOP("  => Executed one task!");
#if RTSX_FIX_TASK_BUG
		if (task->func == read_task_impl_) {
			// free only read_task
			UTL_DEBUG_LOOP("Freeing read task...");
			UTL_FREE(task, sizeof(sdmmc_task));
		}
#endif
	}
#if RTSX_USE_IOLOCK
	IORecursiveLockUnlock(sc->splsdmmc_rec_lock);
#endif
	UTL_DEBUG_DEF("  <=== RELEASING TASK WORKLOOP...");
}

void Sinetek_rtsx::cardEject()
{
	::rtsx_card_eject(rtsx_softc_original_);
}

/**
 *  Attach the macOS portion of the driver: block storage.
 *
 *  That block device will hand us back calls such as read/write blk.
 */
void Sinetek_rtsx::blk_attach()
{
	// TODO: Return an error when this method fails!
	printf("rtsx: blk_attach()\n");

	sddisk_ = OSTypeAlloc(SDDisk); // equivalent to new SDDisk();
	UTL_CHK_PTR(sddisk_,);
	if (!sddisk_->init((struct sdmmc_softc *) rtsx_softc_original_->sdmmc)) { // TODO: Fix this!
		UTL_SAFE_RELEASE_NULL(sddisk_);
		return;
	};
	sddisk_->attach(this);
	UTL_DEBUG_DEF("Registering service...");
	sddisk_->registerService(); // this should probably be called by the start() method of sddisk_
	sddisk_->release();
}

void Sinetek_rtsx::blk_detach()
{
	UTL_DEBUG_FUN("START");

	if (!sddisk_->terminate()) {
		UTL_DEBUG_DEF("sddisk->terminate() returns false!");
	}
	sddisk_ = NULL;
	UTL_DEBUG_FUN("END");
}

#if RTSX_USE_IOFIES
uint32_t Sinetek_rtsx::READ4(IOByteCount offset)
{
	uint32_t ret = 0;
	this->memory_descriptor_->readBytes(offset, &ret, 4);
	// we cannot log here
	return ret;
}

/// This function runs in interrupt context, meaning that IOLog CANNOT be used (only basic functionality is available).
bool Sinetek_rtsx::is_my_interrupt(OSObject *arg, IOFilterInterruptEventSource *source)
{
	// Can't use UTL_CHK_PTR here because we can't log
	if (!arg) return false;

	Sinetek_rtsx *sc = OSDynamicCast(Sinetek_rtsx, arg);
	if (!sc) return false;

	auto status = sc->READ4(RTSX_BIPR);
	if (!status) {
		return false;
	}

	auto bier = sc->READ4(RTSX_BIER);
	if ((status & bier) == 0) return false;

	return true;
}
#endif // RTSX_USE_IOFIES


#if RTSX_USE_IOCOMMANDGATE
void Sinetek_rtsx::executeOneAsCommand() {
	UTL_DEBUG_DEF("Calling runAction()...");
	task_command_gate_->runCommand(nullptr);
	task_command_gate_->runAction((IOCommandGate::Action) executeOneCommandGateAction);
	UTL_DEBUG_DEF("runAction() returns");
}

IOReturn Sinetek_rtsx::executeOneCommandGateAction(OSObject *obj,
						 void *,
						 void *, void *, void * )
{
	UTL_DEBUG_DEF("EXECUTE ONE COMMANDGATEACTION CALLED!");
	rtsx_softc::task_execute_one_impl_(obj, nullptr /* unused */);
	return kIOReturnSuccess;
	UTL_DEBUG_DEF("EXECUTE ONE COMMANDGATEACTION RETURNING");
}
#endif // RTSX_USE_IOCOMMANDGATE

bool Sinetek_rtsx::cardIsWriteProtected() {
	auto bipr = ::bus_space_read_4(gBusSpaceTag,
				       (bus_space_handle_t) memory_descriptor_, RTSX_BIPR);
	return (bipr & RTSX_SD_WRITE_PROTECT) != 0;
}


bool Sinetek_rtsx::writeEnabled()
{
	return write_enabled_;
}
