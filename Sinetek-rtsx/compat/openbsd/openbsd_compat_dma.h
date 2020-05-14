#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_DMA_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_DMA_H

#include "openbsd_compat_types.h" // types

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x0000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x0001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x0002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x0004	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x0010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x0020
#define	BUS_DMA_32BIT		0x0040
#define	BUS_DMA_24BIT		0x0080	/* isadma map */
#define	BUS_DMA_STREAMING	0x0100	/* hint: sequential, unidirectional */
#define	BUS_DMA_READ		0x0200	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x0400	/* mapping is memory -> device only */
#define	BUS_DMA_NOCACHE		0x0800	/* map memory uncached */
#define	BUS_DMA_ZERO		0x1000	/* zero memory in dmamem_alloc */
#define	BUS_DMA_64BIT		0x2000	/* device handles 64bit dva */

/*
 * Operations performed by bus_dmamap_sync().
 */
#define BUS_DMASYNC_PREREAD	0x01
#define BUS_DMASYNC_POSTREAD	0x02
#define BUS_DMASYNC_PREWRITE	0x04
#define BUS_DMASYNC_POSTWRITE	0x08

#ifdef __cplusplus
extern "C" {
#endif

extern bus_space_tag_t    gBusSpaceTag;

extern bus_dma_tag_t      gBusDmaTag;

/// Allocate and initialize a DMA handle
int
bus_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments, bus_size_t maxsegsz,
		  bus_size_t boundary, int flags, bus_dmamap_t *dmamp);

/// Free all resources associated with a given DMA handle
void
bus_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmamp);

/// Loads a DMA handle with mappings for a DMA transfer.
/// It assumes that all pages involved in a DMA transfer are wired.
int
bus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf, bus_size_t buflen, struct proc *p, int flags);

/// Delete the mappings for a given DMA handle
void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map);

/// Perform pre- and post-DMA operation cache and/or buffer synchronization
void
bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t dmam, bus_addr_t offset, bus_size_t size, int ops);

/// Allocate some DMA-able memory. All pages allocated by bus_dmamem_alloc() will be wired down until they are freed by
/// bus_dmamem_free().
int
bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment, bus_size_t boundary, bus_dma_segment_t *segs,
		 int nsegs, int *rsegs, int flags);

/// Free memory previously allocated by bus_dmamem_alloc(), invalidating any mapping
void
bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs);

/// Map DMA memory into kernel's address space
int
bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap, int flags);

/// Unmaps memory previously mapped with bus_dmamem_map(), freeing the kernel virtual address space used by the mapping.
void
bus_dmamem_unmap(bus_dma_tag_t tag, void *kva, size_t size);

#ifdef __cplusplus
}
#endif

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_DMA_H
