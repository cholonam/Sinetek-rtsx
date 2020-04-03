#include <IOKit/IOLib.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOMemoryDescriptor.h>

#include "SDDisk.hpp"
#include "Sinetek_rtsx.hpp"
#include "sdmmcvar.h"
#include "device.h"

#define UTL_THIS_CLASS "SDDisk::"
#include "util.h"
#define printf(...) do {} while (0) /* disable exessive logging */

// Define the superclass
#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(SDDisk, IOBlockStorageDevice)

/// This is a nub, and a nub should be quite stupid, but this is not. Check:
/// https://github.com/zfsrogue/osx-zfs-crypto/blob/master/module/zfs/zvolIO.cpp -> A good nub, it just gets a state
/// from the provider (no memory allocations or resources needed. Just forward everything to the provider (except the
/// doAsyncReadWrite() method).
/// Also:
/// https://opensource.apple.com/source/IOATABlockStorage/IOATABlockStorage-112.0.1/ ->
/// IOATABlockStorage(Device/Driver) is another good example (note that IOATABlockStorageDriver does not inherit from
/// IOBlockStorageDriver, because it's not the same (IOATABlockStorageDriver is the provider of IOATABlockStorageDriver,
/// but IOBlockStorageDriver is the CLIENT of IOBlockStorageDevice). This one DOES forward doAsyncReadWrite.

bool SDDisk::init(struct sdmmc_softc *sc_sdmmc, OSDictionary* properties)
{
	UTL_DEBUG(0, "START");
	if (super::init(properties) == false)
		return false;
	
	util_lock_ = nullptr;
	
	UTL_DEBUG(0, "END");
	return true;
}

bool SDDisk::attach(IOService* provider)
{
	UTL_LOG("SDDisk attaching...");
	if (super::attach(provider) == false)
		return false;
	
	provider_ = OSDynamicCast(rtsx_softc, provider);
	if (provider_ == NULL)
		return false;
	
	provider_->retain();
	
	num_blocks_ = provider_->sc_fn0->csd.capacity;
	blk_size_   = provider_->sc_fn0->csd.sector_size;
	util_lock_ = IOLockAlloc();
	
	printf("rtsx: attaching SDDisk, num_blocks:%d  blk_size:%d\n",
	       num_blocks_, blk_size_);
	
	UTL_LOG("SDDisk attached.");
	return true;
}

void SDDisk::detach(IOService* provider)
{
	UTL_LOG("SDDisk detaching...");
	IOLockFree(util_lock_);
	util_lock_ = NULL;
	provider_->release();
	provider_ = NULL;
	
	super::detach(provider);
	UTL_LOG("SDDisk detached.");
}

IOReturn SDDisk::doEjectMedia(void)
{
	UTL_DEBUG(0, "START");
	IOLog("%s: RAMDISK: doEjectMedia.", __func__);
	
	// XXX signal intent further down the stack?
	// syscl - implement eject routine here
	rtsx_card_eject(provider_);
	UTL_DEBUG(0, "END");
	return kIOReturnSuccess;
}

IOReturn SDDisk::doFormatMedia(UInt64 byteCapacity)
{
	UTL_DEBUG(0, "START");
	return kIOReturnSuccess;
}

UInt32 SDDisk::GetBlockCount() const
{
	UTL_DEBUG(0, "START");
	return num_blocks_;
}

UInt32 SDDisk::doGetFormatCapacities(UInt64* capacities, UInt32 capacitiesMaxCount) const
{
	UTL_DEBUG(0, "START");
	// Ensure that the array is sufficient to hold all our formats (we require 1 element).
	if ((capacities != NULL) && (capacitiesMaxCount < 1))
		return 0;               // Error, return an array size of 0.
	
	/*
	 * We need to run circles around the const-ness of this function.
	 */
	//	auto blockCount = const_cast<SDDisk *>(this)->getBlockCount();
	auto blockCount = GetBlockCount();
	
	// The caller may provide a NULL array if it wishes to query the number of formats that we support.
	if (capacities != NULL)
		capacities[0] = blockCount * blk_size_;
	return 1;
}

IOReturn SDDisk::doLockUnlockMedia(bool doLock)
{
	UTL_DEBUG(0, "START");
	return kIOReturnUnsupported;
}

IOReturn SDDisk::doSynchronizeCache(void)
{
	UTL_DEBUG(0, "START");
	return kIOReturnSuccess;
}

char* SDDisk::getVendorString(void)
{
	UTL_DEBUG(0, "START");
	// syscl - safely converted to char * use const_static due
	// to ISO C++11 does not allow conversion from string literal to 'char *'
	return const_cast<char *>("Realtek");
}

char* SDDisk::getProductString(void)
{
	UTL_DEBUG(0, "START");
	// syscl - safely converted to char * use const_static due
	// to ISO C++11 does not allow conversion from string literal to 'char *'
	return const_cast<char *>("SD Card Reader");
}

char* SDDisk::getRevisionString(void)
{
	UTL_DEBUG(0, "START");
	// syscl - safely converted to char * use const_static due
	// to ISO C++11 does not allow conversion from string literal to 'char *'
	return const_cast<char *>("1.0");
}

char* SDDisk::getAdditionalDeviceInfoString(void)
{
	UTL_DEBUG(0, "START");
	// syscl - safely converted to char * use const_static due
	// to ISO C++11 does not allow conversion from string literal to 'char *''
	return const_cast<char *>("1.0");
}

IOReturn SDDisk::reportBlockSize(UInt64 *blockSize)
{
	UTL_DEBUG(0, "START");
	if (blk_size_ != 512) {
		UTL_DEBUG(0, "Reporting block size != 512 (%u)", blk_size_);
	}
	*blockSize = blk_size_;
	return kIOReturnSuccess;
}

IOReturn SDDisk::reportEjectability(bool *isEjectable)
{
	UTL_DEBUG(0, "START");
	*isEjectable = true; // syscl - should be true
	return kIOReturnSuccess;
}

/* syscl - deprecated
 IOReturn SDDisk::reportLockability(bool *isLockable)
 {
 *isLockable = false;
 return kIOReturnSuccess;
 }*/

IOReturn SDDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	UTL_DEBUG(0, "Called (maxBlock = %u)", num_blocks_ - 1);
	*maxBlock = num_blocks_ - 1;
	return kIOReturnSuccess;
}

IOReturn SDDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
	UTL_DEBUG(0, "START");
	*mediaPresent = true;
	*changedState = false;
	
	return kIOReturnSuccess;
}

IOReturn SDDisk::reportPollRequirements(bool *pollRequired, bool *pollIsExpensive)
{
	UTL_DEBUG(0, "START");
	*pollRequired = false;
	*pollIsExpensive = false;
	return kIOReturnSuccess;
}

IOReturn SDDisk::reportRemovability(bool *isRemovable)
{
	UTL_DEBUG(0, "START");
	*isRemovable = true;
	return kIOReturnSuccess;
}

IOReturn SDDisk::reportWriteProtection(bool *isWriteProtected)
{
	UTL_DEBUG(0, "START");
	*isWriteProtected = false; // XXX
	return kIOReturnSuccess;
}

IOReturn SDDisk::getWriteCacheState(bool *enabled)
{
	UTL_DEBUG(0, "START");
	return kIOReturnUnsupported;
}

IOReturn SDDisk::setWriteCacheState(bool enabled)
{
	UTL_DEBUG(0, "START");
	return kIOReturnUnsupported;
}


struct BioArgs
{
	IOMemoryDescriptor *buffer;
	IODirection direction;
	UInt64 block;
	UInt64 nblks;
	IOStorageAttributes attributes;
	IOStorageCompletion completion;
	SDDisk *that;
};

// move me
int
sdmmc_mem_read_block(struct sdmmc_function *sf, int blkno, u_char *data,
		     size_t datalen);
int
sdmmc_mem_single_read_block(struct sdmmc_function *sf, int blkno, u_char *data,
			    size_t datalen);
void
sdmmc_add_task(struct sdmmc_softc *sc, struct sdmmc_task *task);
void
sdmmc_go_idle_state(struct sdmmc_softc *sc);
int
sdmmc_mem_read_block_subr(struct sdmmc_function *sf,
			  int blkno, u_char *data, size_t datalen);


// cholonam: This task is put on a queue which is run by sc::task_execute_one_ (originally using a timer, now trying to
// change to an IOCommandGate.
void read_task_impl_(void *_args)
{
	BioArgs *args = (BioArgs *) _args;
	IOByteCount actualByteCount;
	int error = 0;
	
	UTL_CHK_PTR(args,);
	UTL_CHK_PTR(args->buffer,);
	UTL_CHK_PTR(args->that,);
	UTL_CHK_PTR(args->that->provider_,);
	UTL_CHK_PTR(args->that->provider_->sc_fn0,);
	UTL_DEBUG(0, "START (block = %u nblks = %u blksize = %u)",
		  static_cast<unsigned>(args->block),
		  static_cast<unsigned>(args->nblks),
		  args->that->blk_size_);
	
	printf("read_task_impl_  sz %llu\n", args->nblks * args->that->blk_size_);
	printf("sf->csd.sector_size %d\n", args->that->provider_->sc_fn0->csd.sector_size);
	
	
	actualByteCount = args->nblks * args->that->blk_size_;
	IOByteCount maxSendBytes = 128 * 1024;
	IOByteCount remainingBytes = args->nblks * 512;
	IOByteCount sentBytes = 0;
	int blocks = (int) args->block;

#if RTSX_USE_WRITEBYTES
	u_char *buf = new u_char[actualByteCount];
#else
	auto map = args->buffer->map();
	u_char *buf = (u_char *) map->getAddress();
#endif

	while (remainingBytes > 0) {
		IOByteCount sendByteCount = remainingBytes > maxSendBytes ? maxSendBytes : remainingBytes;

		if (args->direction == kIODirectionIn) {
			error = sdmmc_mem_read_block(args->that->provider_->sc_fn0, blocks, buf + sentBytes, sendByteCount);
		}
		else {

#if RTSX_USE_WRITEBYTES
			IOByteCount copied_bytes = args->buffer->readBytes(sentBytes, buf, sendByteCount);
			if (copied_bytes == 0) {
				delete[] buf;
				if (args->completion.action) {
					(args->completion.action)(args->completion.target, args->completion.parameter, kIOReturnIOError, 0);
				}
				goto out;
			}
#endif
			error = sdmmc_mem_write_block(args->that->provider_->sc_fn0, blocks, buf + sentBytes, sendByteCount);
		}
		if (error) {
			if (args->completion.action) {
				(args->completion.action)(args->completion.target, args->completion.parameter,
							  kIOReturnIOError, 0);
			} else {
				UTL_ERR("No completion action!");
			}
			goto out;
		}

#if RTSX_USE_WRITEBYTES
		IOByteCount copied_bytes = args->buffer->writeBytes(sentBytes, buf, sendByteCount);
		if (copied_bytes == 0) {
			delete[] buf;
			if (args->completion.action) {
				(args->completion.action)(args->completion.target, args->completion.parameter, kIOReturnIOError, 0);
			}
			goto out;
		}
#endif
		blocks += (sendByteCount / 512);
		remainingBytes -= sendByteCount;
		sentBytes += sendByteCount;
	}

#if RTSX_USE_WRITEBYTES
	delete[] buf;
#else
	map->release();
#endif
	if (args->completion.action) {
		(args->completion.action)(args->completion.target, args->completion.parameter,
					  kIOReturnSuccess, actualByteCount);
	} else {
		UTL_ERR("No completion action!");
	}
	
out:
	if (error) {
		UTL_ERR("END (error = %d)", error);
	} else {
		UTL_DEBUG(0, "END (error = %d)", error);
	}
	delete args;
}

/**
 * Start an async read or write operation.
 * @param buffer
 * An IOMemoryDescriptor describing the data-transfer buffer. The data direction
 * is contained in the IOMemoryDescriptor. Responsibility for releasing the descriptor
 * rests with the caller.
 * @param block
 * The starting block number of the data transfer.
 * @param nblks
 * The integral number of blocks to be transferred.
 * @param attributes
 * Attributes of the data transfer. See IOStorageAttributes.
 * @param completion
 * The completion routine to call once the data transfer is complete.
 */
IOReturn SDDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer,
				  UInt64 block,
				  UInt64 nblks,
				  IOStorageAttributes *attributes,
				  IOStorageCompletion *completion)
{
	IODirection		direction;
	
	// Return errors for incoming I/O if we have been terminated
	if (isInactive() != false)
		return kIOReturnNotAttached;
	
	direction = buffer->getDirection();
	if ((direction != kIODirectionIn) && (direction != kIODirectionOut))
		return kIOReturnBadArgument;
	
	/*
	if (direction == kIODirectionOut)
		return kIOReturnNotWritable; // read-only driver for now...
	*/

	if ((block + nblks) > num_blocks_)
		return kIOReturnBadArgument;
	
	if ((direction != kIODirectionIn) && (direction != kIODirectionOut))
		return kIOReturnBadArgument;
	
	// printf("=====================================================\n");
	UTL_DEBUG(0, "START (block=%d nblks=%d)", (int) block, (int) nblks);
	IOLockLock(util_lock_);
	
	/*
	 * Copy things over as we're going to lose the parameters once this
	 * method returns. (async call)
	 */
	BioArgs *bioargs = new BioArgs;
	bioargs->buffer = buffer;
	bioargs->direction = direction;
	bioargs->block = block;
	bioargs->nblks = nblks;
	if (attributes != nullptr)
		bioargs->attributes = *attributes;
	if (completion != nullptr)
		bioargs->completion = *completion;
	bioargs->that = this;
	
#if RTSX_FIX_TASK_BUG
	UTL_DEBUG(0, "Allocating read task...");
	auto newTask = UTL_MALLOC(sdmmc_task); // will be deleted after processed
	if (!newTask) return kIOReturnNoMemory;
	sdmmc_init_task(newTask, read_task_impl_, bioargs);
	sdmmc_add_task(provider_, newTask);
#else
	sdmmc_init_task(&provider_->read_task_, read_task_impl_, bioargs);
	sdmmc_add_task(provider_, &provider_->read_task_);
#endif
	
	IOLockUnlock(util_lock_);
	// printf("=====================================================\n");
	
	UTL_DEBUG(0, "RETURNING SUCCESS");
	return kIOReturnSuccess;
}
