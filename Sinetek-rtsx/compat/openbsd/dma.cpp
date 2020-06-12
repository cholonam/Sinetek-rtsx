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

// This struct MUST match bus_dma_segment_t
typedef struct {
	uint64_t			ds_addr;
	uint64_t			ds_len;
	// We store the IOMemoryDescriptor and IOMemoryMap here because we have no other place to store them.
	// They are store only in the first element of segs (we only support one segment anyway).
	IOBufferMemoryDescriptor *	_ds_memDesc;
	IOMemoryMap *			_ds_memMap;
} _bus_dma_segment_t;

_bus_dma_tag _busDmaTag = {};
bus_space_tag_t gBusSpaceTag = {};
bus_dma_tag_t gBusDmaTag = (bus_dma_tag_t) &_busDmaTag;

// class to keep track of the segments belonging to a virtual address
typedef StaticDictionary<void *, _bus_dma_segment_t *> VA_SEGS;
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

int
bus_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments, bus_size_t maxsegsz,
		  bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	UTL_DEBUG_FUN("START");
	if (nsegments != 1) {
		UTL_ERR("Only one single segment supported for now");
		return ENOTSUP;
	}
	UTL_CHK_PTR(dmamp, EINVAL);

	bus_dmamap_t ret = UTL_MALLOC(bus_dmamap);
	UTL_CHK_PTR(ret, ENOMEM);

	bzero(ret, sizeof(bus_dmamap));
	ret->_dm_size = size;
	ret->_dm_segcnt = nsegments;
	ret->_dm_maxsegsz = maxsegsz;
	ret->_dm_boundary = boundary;
	ret->_dm_flags = flags;

	*dmamp = ret;
	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmamp)
{
	UTL_DEBUG_FUN("START");
	UTL_FREE(dmamp, _bus_dmamap);
	UTL_DEBUG_FUN("END");
}

int
bus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf, bus_size_t buflen, struct proc *p, int flags)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(dmam, EINVAL);
	if (p != nullptr) {
		return ENOTSUP; // only kernel space supported
	}

	_bus_dma_segment_t *segs;
	if (VA_SEGS::getValueFromList(buf, &segs)) {
		UTL_ERR("Could not get segments from virtual address!");
		return ENOTSUP;
	}

	// load segment information into dmam:
	dmam->dm_mapsize = dmam->_dm_size;
	dmam->dm_nsegs = 1;
	dmam->dm_segs[0].ds_addr = segs[0].ds_addr;
	dmam->dm_segs[0].ds_len = segs[0].ds_len;
	dmam->dm_segs[0]._ds_memDesc = segs[0]._ds_memDesc;
	dmam->dm_segs[0]._ds_memMap = segs[0]._ds_memMap;
	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	UTL_DEBUG_FUN("START");
	// TODO: What should be done here?
	UTL_DEBUG_FUN("END");
	return;
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

int
bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment, bus_size_t boundary, bus_dma_segment_t *segs,
		 int nsegs, int *rsegs, int flags)
{
	UTL_DEBUG_FUN("START");
	if (nsegs != 1) return ENOTSUP; // only one supported for now
	_bus_dma_segment_t *_segs = reinterpret_cast<_bus_dma_segment_t *>(segs);

	UTL_CHK_PTR(tag, EINVAL);
	UTL_CHK_PTR(_segs, EINVAL);
	UTL_CHK_PTR(rsegs, EINVAL);

	// allocate srange of physical memory (virtual too?)
	auto *memDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
									 kIODirectionInOut |
									 kIOMemoryPhysicallyContiguous |
									 kIOMapInhibitCache,
									 size,
									 0x00000000ffffffffull);
	if (!memDesc) {
		UTL_ERR("Could not allocate %d bytes of DMA (memDesc is null)!", (int) size);
		return ENOMEM;
	}

	IOByteCount len;
	auto physAddr = memDesc->getPhysicalSegment(0, &len);
	if (len != size || !physAddr) {
		UTL_ERR("len=%d, size=%d, addr=" RTSX_PTR_FMT, (int) len, (int) size, RTSX_PTR_FMT_VAR(physAddr));
		UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
		return ENOTSUP;
	}
	UTL_DEBUG_MEM("Allocated 0x%x bytes @ physical address 0x%04x", (int) len, (int) physAddr);

	// call prepare here? does this wire the pages?
	if (UTL_CHK_SUCCESS(memDesc->prepare(kIODirectionNone))) { // kIODirectionNone use descriptor's direction
		UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
		return ENOMEM;
	}

	// return physical address
	_segs[0].ds_addr = physAddr;
	_segs[0].ds_len = len;
	_segs[0]._ds_memDesc = memDesc;
	_segs[0]._ds_memMap = nullptr; // check all members are initialized

	*rsegs = 1;
	UTL_DEBUG_FUN("END");
	return 0;
}

void
bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	UTL_DEBUG_FUN("START");
	UTL_CHK_PTR(segs,);
	if (nsegs != 1) return; // only one supported for now
	_bus_dma_segment_t *_segs = reinterpret_cast<_bus_dma_segment_t *>(segs);
	auto &memDesc = _segs[0]._ds_memDesc; // we use a referece because we will set it to null
	UTL_CHK_PTR(memDesc,);

	// complete and release
	memDesc->complete(kIODirectionInOut);
	UTL_DEBUG_MEM("Freeing memory @ physical address 0x%04x", (int) _segs[0].ds_addr);
	UTL_SAFE_RELEASE_NULL_CHK(memDesc, 1);
	UTL_DEBUG_FUN("END");
}

int
bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap, int flags)
{
	UTL_DEBUG_FUN("START");
	if (nsegs != 1) return ENOTSUP; // only one supported for now
	_bus_dma_tag *_tag = reinterpret_cast<_bus_dma_tag *>(tag);
	_bus_dma_segment_t *_segs = reinterpret_cast<_bus_dma_segment_t *>(segs);
	UTL_CHK_PTR(_tag, EINVAL);
	UTL_CHK_PTR(_segs, EINVAL);
	UTL_CHK_PTR(kvap, EINVAL);

	auto memDesc = _segs[0]._ds_memDesc;
	if (!memDesc) return EINVAL;
	if (_segs[0]._ds_memMap) {
		UTL_ERR("_ds_memMap should be null but it's not!");
		return ENOTSUP;
	}

	_segs[0]._ds_memMap = memDesc->map();
	if (!_segs[0]._ds_memMap) {
		UTL_ERR("memoryDescriptor->map() returned null!");
		return ENOMEM;
	}

	auto virtAddr = _segs[0]._ds_memMap->getAddress(); // getVirtualAddress();
	*kvap = (caddr_t) virtAddr;

	// update list
	if (VA_SEGS::addToList((void *) virtAddr, _segs)) {
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
	_bus_dma_segment_t *segs;
	if (VA_SEGS::getValueFromList(kva, &segs)) {
		UTL_ERR("Could not get segments from virtual address!");
		return;
	}
	// release memory map
	auto &memMap = segs[0]._ds_memMap;
	UTL_CHK_PTR(memMap,);
	UTL_SAFE_RELEASE_NULL_CHK(memMap, 1);
	// remove from list
	VA_SEGS::removeFromList(kva);
	UTL_DEBUG_FUN("END");
}
