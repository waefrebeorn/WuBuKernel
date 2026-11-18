/*
 * WuBuKernel physical memory manager -- bitmap over boot-supplied regions.
 *
 * Frames are numbered region-locally (base + index*4096).  Region 0 starts at
 * index 0; later regions continue the same index space, which keeps alloc/free
 * a single table walk with no searching.
 */
#include "wubukernel_pmm.h"

#define PMM_PAGE_BITS (WUBU_PAGE_SIZE / 8)

static int page_to_bit(const wubu_pmm *p, uint64_t page, uint64_t *bit)
{
    if (page % WUBU_PAGE_SIZE) return -1;
    uint64_t idx = 0;
    for (int r = 0; r < p->nregions; ++r) {
        const wubu_pmm_region *rg = &p->regions[r];
        if (page < rg->base) return -1;
        uint64_t off = page - rg->base;
        if (off / WUBU_PAGE_SIZE < rg->pages) {
            *bit = idx + off / WUBU_PAGE_SIZE;
            return 0;
        }
        idx += rg->pages;
    }
    return -1;
}

int wubu_pmm_init(wubu_pmm *p, const wubu_pmm_region *regions, int nregions,
                  uint8_t *bitmap, size_t bitmap_bytes)
{
    if (!p || !regions || nregions <= 0 || nregions > WUBU_PMM_MAX_REGIONS ||
        !bitmap)
        return -1;

    p->nregions = nregions;
    p->total_pages = 0;
    for (int i = 0; i < nregions; ++i) {
        wubu_pmm_region rg = regions[i];
        /* normalise: drop unaligned head/tail instead of misreporting frames */
        uint64_t base = (rg.base + (WUBU_PAGE_SIZE - 1)) & ~(uint64_t)(WUBU_PAGE_SIZE - 1);
        if (rg.pages && base < rg.base + rg.pages * WUBU_PAGE_SIZE) {
            uint64_t avail = (rg.base + rg.pages * WUBU_PAGE_SIZE - base)
                             / WUBU_PAGE_SIZE;
            rg.base = base;
            rg.pages = avail;
        } else {
            rg.pages = 0;
        }
        p->regions[i] = rg;
        p->total_pages += rg.pages;
    }

    size_t need = (size_t)((p->total_pages + 7) / 8);
    if (bitmap_bytes < need) return -1;
    for (size_t i = 0; i < need; ++i) bitmap[i] = 0;

    p->bitmap = bitmap;
    p->bitmap_bytes = need;
    p->free_pages = p->total_pages;
    return 0;
}

uint64_t wubu_pmm_alloc(wubu_pmm *p)
{
    if (!p || p->free_pages == 0) return 0;
    for (uint64_t b = 0; b < p->total_pages; ++b) {
        size_t byte = (size_t)(b / 8);
        unsigned mask = 1u << (b % 8);
        if (!(p->bitmap[byte] & mask)) {
            p->bitmap[byte] |= (uint8_t)mask;
            p->free_pages--;

            /* map index back to a physical address */
            uint64_t idx = 0;
            for (int r = 0; r < p->nregions; ++r) {
                if (b < idx + p->regions[r].pages)
                    return p->regions[r].base + (b - idx) * WUBU_PAGE_SIZE;
                idx += p->regions[r].pages;
            }
            return 0;   /* unreachable: index came from the same table */
        }
    }
    return 0;
}

void wubu_pmm_free(wubu_pmm *p, uint64_t page)
{
    uint64_t bit;
    if (!p || page_to_bit(p, page, &bit) != 0) return;   /* foreign frame */
    size_t byte = (size_t)(bit / 8);
    unsigned mask = 1u << (bit % 8);
    if (p->bitmap[byte] & mask) {
        p->bitmap[byte] &= (uint8_t)~mask;
        p->free_pages++;
    }
}

int wubu_pmm_is_allocated(const wubu_pmm *p, uint64_t page)
{
    uint64_t bit;
    if (!p || page_to_bit(p, page, &bit) != 0) return -1;
    return (p->bitmap[bit / 8] >> (bit % 8)) & 1u;
}

uint64_t wubu_pmm_total(const wubu_pmm *p) { return p ? p->total_pages : 0; }
uint64_t wubu_pmm_free_count(const wubu_pmm *p) { return p ? p->free_pages : 0; }
