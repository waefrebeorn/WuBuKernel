#ifndef WUBUKERNEL_PMM_H
#define WUBUKERNEL_PMM_H

/*
 * WuBuKernel physical memory manager.
 *
 * One concern: which physical frames exist and which are free.  A bitmap over
 * caller-supplied regions (from the boot memory map); no ownership of the
 * memory itself.  Freestanding C11.
 */

#include <stddef.h>
#include <stdint.h>

#define WUBU_PMM_MAX_REGIONS 8
#define WUBU_PAGE_SIZE       4096u

typedef struct {
    uint64_t base;      /* page-aligned */
    uint64_t pages;
} wubu_pmm_region;

typedef struct {
    wubu_pmm_region regions[WUBU_PMM_MAX_REGIONS];
    int nregions;
    uint64_t total_pages;
    uint64_t free_pages;
    uint8_t *bitmap;            /* 1 = allocated; owned by the caller */
    size_t bitmap_bytes;
} wubu_pmm;

/* Install a region list and a bitmap buffer.  Returns 0 on success. */
int  wubu_pmm_init(wubu_pmm *p, const wubu_pmm_region *regions, int nregions,
                   uint8_t *bitmap, size_t bitmap_bytes);

/* Allocate one page.  Returns page-aligned physical address, or 0 when the
 * pool is exhausted.  Never hands out a frame twice. */
uint64_t wubu_pmm_alloc(wubu_pmm *p);
void     wubu_pmm_free(wubu_pmm *p, uint64_t page);

int      wubu_pmm_is_allocated(const wubu_pmm *p, uint64_t page);
uint64_t wubu_pmm_total(const wubu_pmm *p);
uint64_t wubu_pmm_free_count(const wubu_pmm *p);

#endif /* WUBUKERNEL_PMM_H */
