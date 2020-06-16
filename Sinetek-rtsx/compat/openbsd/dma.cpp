#include "dma.h"

#include <libkern/OSAtomic.h> // OSSynchronizeIO
#include <sys/errno.h>
#include <string.h> // bzero
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOMemoryDescriptor.h> // IOMemoryMap

#define UTL_THIS_CLASS ""
#include "util.h"

typedef struct {
	// IOBufferMemoryDescriptor *memoryDescriptor;
	// IOMemoryMap              *memoryMap;
} _bus_dma_tag;

_bus_dma_tag _busDmaTag = {};
bus_space_tag_t gBusSpaceTag = {};
bus_dma_tag_t gBusDmaTag = (bus_dma_tag_t) &_busDmaTag;

/// class to keep track of the segments belonging to a virtual address
typedef StaticDictionary<void *, bus_dma_segment_t *> VA_SEGS;
UTL_STATIC_DICT_INIT(VA_SEGS);

// bus_dmamap_create();         /* get a dmamap to load/unload          */
// for each DMA xfer {
//         bus_dmamem_alloc();  /* allocate some DMA'able memory        */
//         bus_dmamem_map();    /* map it into the kernel address space */
//         /* Fill the allocated DMA'able memory with whatever data
//          * is to be sent out, using the pointer obtained with
//          * bus_dmamem_map(). */
//         bus_dmamap_load();   /* initialize the segments of dmamap    */
//         bus_dmamap_sync();   /* synchronize/flush any DMA cache      */
//         for (i = 0; i < dm_nsegs; i++) {
//                 /* Tell the DMA device the physical address
//                  * (dmamap->dm_segs[i].ds_addr) and the length
//                  * (dmamap->dm_segs[i].ds_len) of the memory to xfer.
//                  *
//                  * Start the DMA, wait until it's done */
//         }
//         bus_dmamap_sync();   /* synchronize/flush any DMA cache      */
//         bus_dmamap_unload(); /* prepare dmamap for reuse             */
//         /* Copy any data desired from the DMA'able memory using the
//          * pointer created by bus_dmamem_map(). */
//         bus_dmamem_unmap();  /* free kernel virtual address space    */
//         bus_dmamem_free();   /* free DMA'able memory                 */
// }
// bus_dmamap_destroy();        /* release any resources used by dmamap */

// See: https://github.com/openbsd/src/blob/master/sys/arch/amd64/amd64/bus_dma.c

int
bus_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments, bus_size_t maxsegsz,
		  bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	UTL_DEBUG_FUN("START (nsegments=%d maxsegsz=%lu boundary=%lu flags=%d", nsegments, maxsegsz, boundary, flags);
	UTL_CHK_PTR(dmamp, EINVAL);

	size_t mapsize = sizeof(struct bus_dmamap) + sizeof(bus_dma_segment_t) * (nsegments - 1);

	bus_dmamap_t ret = (bus_dmamap_t)IOMalloc(mapsize);
	UTL_CHK_PTR(ret, ENOMEM);

	bzero(ret, sizeof(bus_dmamap));
	ret->_dm_size = size;
	ret->_dm_segcnt = nsegments;
	ret->_dm_maxsegsz = maxsegsz;
	ret->_dm_boundary = boundary;
	ret->_dm_flags = flags;

	// Pre-create IODMACommand for reuse
	IODMACommand::SegmentOptions segmentOptions = { .fStructSize = sizeof(segmentOptions),
							.fNumAddressBits = 32,
							.fMaxSegmentSize = maxsegsz,
							.fMaxTransferSize = 0,
							.fAlignment = 1,
							.fAlignmentLength = 1,
							.fAlignmentInternalSegments = (uint32_t)boundary };
	IODMACommand *dmaCmd = IODMACommand::withSpecification(kIODMACommandOutputHost32, &segmentOptions,
							       kIODMAMapOptionMapped, nullptr, nullptr);
	ret->_dm_dma_command = dmaCmd;
	*dmamp = ret;
	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmamp)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(dmamp, );
	UTL_SAFE_RELEASE_NULL_CHK(dmamp->_dm_dma_command, 1);
	size_t mapsize = sizeof(struct bus_dmamap) + sizeof(bus_dma_segment_t) * (dmamp->_dm_segcnt - 1);
	IOFree(dmamp, mapsize);
	UTL_DEBUG_FUN("END");
}

int
bus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf, bus_size_t buflen, struct proc *p, int flags)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(dmam, EINVAL);
	UTL_CHK_PTR(dmam->_dm_dma_command, EINVAL);
	UTL_CHK_PTR(buf, EINVAL);
	if (p != nullptr) {
		UTL_ERR("Only kernel process (NULL) supported!");
		return ENOTSUP; // only kernel space supported
	}

	IOMemoryDescriptor *md;
	IOMemoryMap *       mmap;
	bus_dma_segment_t *segs;
	if (VA_SEGS::getValueFromList(buf, &segs)) {
		UTL_ERR("Could not get segments from virtual address!");
		return ENOTSUP;
	}
	UTL_CHK_PTR(segs, EINVAL);
	UTL_CHK_PTR(segs[0]._ds_memDesc, EINVAL);
	UTL_CHK_PTR(segs[0]._ds_memMap, EINVAL);

	md = segs[0]._ds_memDesc;
	mmap = segs[0]._ds_memMap;

	if (md->getLength() != buflen) {
		UTL_ERR("Memory descriptor and buffer do not match!");
		return ENOTSUP;
	}

	// Use IODMACommand to generate segments
	IODMACommand *dmaCmd = dmam->_dm_dma_command;

	int err;
	if ((err = UTL_CHK_SUCCESS(dmaCmd->setMemoryDescriptor(md))))
		return err;

	IOByteCount offset = 0;
	int         segCnt = 0; // to make sure we can fit the segments in the dmamap
	while (offset < buflen && segCnt < dmam->_dm_segcnt) {
		IODMACommand::Segment32 segment;
		UInt32                  numSeg = 1;
		if ((err = UTL_CHK_SUCCESS(dmaCmd->genIOVMSegments(&offset, &segment, &numSeg))))
			return err;
		dmam->dm_segs[segCnt].ds_addr = segment.fIOVMAddr;
		dmam->dm_segs[segCnt].ds_len = segment.fLength;
		segCnt++;
	}

	// Make sure that everything went ok
	if (offset != buflen) {
		UTL_ERR("Error generating DMA scatter/gather list (offset = %llx, buflen = %lx", offset, buflen);
		return ENOTSUP;
	}

	// load segment information into dmam:
	dmam->dm_mapsize = buflen;
	dmam->dm_nsegs = segCnt;
	dmam->dm_segs[0]._ds_memDesc = md;
	dmam->dm_segs[0]._ds_memMap = mmap;
	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(map, );
	UTL_CHK_PTR(map->_dm_dma_command, );

	map->_dm_dma_command->clearMemoryDescriptor();
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	UTL_DEBUG_FUN("END");
}

void
bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t dmam, bus_addr_t offset, bus_size_t size, int ops)
{
	UTL_DEBUG_FUN("START");
	// This function should probably call prepare() / complete(), but we already call them in
	// bus_dmamem_alloc()/bus_dmamem_free()
	OSSynchronizeIO(); // this is actually a noop on Intel
	UTL_DEBUG_FUN("END");
	return;
}

// alignment and boundary are always zero in the code!
int
bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment, bus_size_t boundary, bus_dma_segment_t *segs,
		 int nsegs, int *rsegs, int flags)
{
	UTL_DEBUG_FUN("START");

	UTL_CHK_PTR(tag, EINVAL);
	UTL_CHK_PTR(segs, EINVAL);
	UTL_CHK_PTR(rsegs, EINVAL);

	IOOptionBits options = kIODirectionInOut | kIOMapInhibitCache;
	if (nsegs == 1) {
		options |= kIOMemoryPhysicallyContiguous;
	}
	// allocate srange of physical memory (virtual too?)
	auto *memDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, options, size, 0xfffff000);
	if (!memDesc) {
		UTL_ERR("Could not allocate %d bytes of DMA (memDesc is null)!", (int) size);
		return ENOMEM;
	}

	// we need to call prepare() so that the pages are not paged-out
	// (we cannot call getPhysicalSegment() before prepare())
	if (UTL_CHK_SUCCESS(memDesc->prepare()) != kIOReturnSuccess) {
		UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
		return -ENOMEM;
	}
	IOByteCount len;
	IOByteCount off = 0;
	int         segIdx = 0;
	auto physAddr = memDesc->getPhysicalSegment(off, &len);
	if ((nsegs == 1 && len != size) || !physAddr) {
		UTL_ERR("len=%d, size=%d, physAddr=%08llx%s", (int) len, (int) size, physAddr,
			physAddr > 0xffffffff ? " (HIGHMEM)" : "");
		UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
		return ENOTSUP;
	}
	// return physical address
	segs[segIdx].ds_addr = physAddr;
	segs[segIdx].ds_len = len;
	segIdx++;

	// At this point, if len < size, we need to return more than one segment
	
	IOByteCount totalLen = len;
	off += len;
	while (totalLen < size) {
		auto physAddr = memDesc->getPhysicalSegment(off, &len);
		if (!physAddr || !len) {
			UTL_ERR("WRONG VALUE RETURNED! (len=%llu totalLen=%llu, size=%lu, addr=" RTSX_PTR_FMT ")", len,
				totalLen, size, RTSX_PTR_FMT_VAR(physAddr));
			UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
			return ENOMEM;
		}
		totalLen += len;
		off += len;
		if (totalLen > size) {
			UTL_ERR("totalLen > size! (len=%llu totalLen=%llu, size=%lu, addr=" RTSX_PTR_FMT ")", len,
				totalLen, size, RTSX_PTR_FMT_VAR(physAddr));
			UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
			return ENOMEM;
		}
		segs[segIdx].ds_addr = physAddr;
		segs[segIdx].ds_len = len;
		segIdx++;
		if (totalLen < size && segIdx > nsegs) {
			// too many segments! we need to return an error
			UTL_ERR("TOO MANY SEGMENTS! (len=%llu totalLen=%llu, size=%lu, addr=" RTSX_PTR_FMT ")", len,
				totalLen, size, RTSX_PTR_FMT_VAR(physAddr));
			UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
			return ENOMEM;
		}
	}
	UTL_DEBUG_MEM("Allocated %ld bytes @ physical address 0x%08llx in %d segments (firstSegLen=%llu)", size,
		      segs[0].ds_addr, segIdx, segs[0].ds_len);
	segs[0]._ds_memDesc = memDesc;
	segs[0]._ds_memMap = nullptr; // check all members are initialized
	*rsegs = segIdx;
	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(segs,);
	auto &memDesc = segs[0]._ds_memDesc; // we use a referece because we will set it to null
	UTL_CHK_PTR(memDesc,);

	// complete and release
	memDesc->complete();
	UTL_DEBUG_MEM("Freeing memory @ physical address 0x%04x", (int) segs[0].ds_addr);
	UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
	UTL_DEBUG_FUN("END");
}

int
bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap, int flags)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(segs, EINVAL);
	UTL_CHK_PTR(kvap, EINVAL);

	auto memDesc = segs[0]._ds_memDesc;
	if (!memDesc) return EINVAL;
	if (segs[0]._ds_memMap) {
		UTL_ERR("_ds_memMap should be null but it's not!");
		return ENOTSUP;
	}

	segs[0]._ds_memMap = memDesc->map();
	if (!segs[0]._ds_memMap) {
		UTL_ERR("memoryDescriptor->map() returned null!");
		return ENOMEM;
	}

	auto virtAddr = segs[0]._ds_memMap->getAddress(); // getVirtualAddress();
	*kvap = (caddr_t) virtAddr;

	// update list
	if (VA_SEGS::addToList((void *) virtAddr, segs)) {
		UTL_ERR("Could not keep track of memory allocation!");
		return ENOTSUP;
	}

	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamem_unmap(bus_dma_tag_t tag, void *kva, size_t size)
{
	UTL_DEBUG_FUN("START");
	bus_dma_segment_t *segs;
	if (VA_SEGS::getValueFromList(kva, &segs)) {
		UTL_ERR("Could not get segments from virtual address!");
		return;
	}
	// release memory map
	auto &memMap = segs[0]._ds_memMap;
	UTL_CHK_PTR(memMap,);
	UTL_SAFE_RELEASE_NULL_CHK(memMap, 2); // because the memory descriptor is holding a reference
	// remove from list
	VA_SEGS::removeFromList(kva);
	UTL_DEBUG_FUN("END");
}

