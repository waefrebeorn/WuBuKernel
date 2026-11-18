/*
 * WuBuKernel PMM gate.
 *
 * Oracles come from the contract in wubukernel_pmm.h: region-local frames in a
 * single index space, bases normalised up to a frame, alloc never repeats a
 * frame and returns 0 on exhaustion, free ignores foreign pages and does not
 * refund an already-free frame.
 */
#include "wubukernel_pmm.h"

#include <stdint.h>
#include <stdio.h>

static int fails;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("  [FAIL] %s\n", (msg));                                   \
            ++fails;                                                          \
        }                                                                     \
    } while (0)

/* Two adjacent regions: 4 frames at 0x1000, then 3 at 0x100000. */
static void test_two_region_index_space(void)
{
    static uint8_t bitmap[8];
    const wubu_pmm_region regions[2] = {
        { 0x1000, 4 },
        { 0x100000, 3 },
    };
    wubu_pmm p;
    CHECK(wubu_pmm_init(&p, regions, 2, bitmap, sizeof bitmap) == 0,
          "init two regions");
    CHECK(wubu_pmm_total(&p) == 7 && wubu_pmm_free_count(&p) == 7,
          "total/free == 7");

    uint64_t a = wubu_pmm_alloc(&p);
    uint64_t b = wubu_pmm_alloc(&p);
    CHECK(a == 0x1000, "first alloc is the first frame of region 0");
    CHECK(b == 0x2000, "second alloc is the second frame");
    CHECK(wubu_pmm_is_allocated(&p, a) == 1, "allocated frame reports allocated");
    CHECK(wubu_pmm_is_allocated(&p, 0x100000) == 0, "untouched frame reports free");

    /* Drain the rest: the boundary must be crossed exactly once. */
    uint64_t seen[7];
    seen[0] = a;
    seen[1] = b;
    for (int i = 2; i < 7; ++i) seen[i] = wubu_pmm_alloc(&p);
    CHECK(seen[4] == 0x100000, "alloc crosses into region 1 at its base");
    CHECK(wubu_pmm_free_count(&p) == 0, "pool drained");

    uint64_t again = wubu_pmm_alloc(&p);
    CHECK(again == 0, "alloc past exhaustion returns 0");

    wubu_pmm_free(&p, b);
    wubu_pmm_free(&p, b);                       /* idempotent refund guard */
    CHECK(wubu_pmm_free_count(&p) == 1, "double free does not over-refund");
    CHECK(wubu_pmm_alloc(&p) == 0x2000, "released frame comes back");
}

/* Foreign and unaligned addresses are ignored rather than corrupting state. */
static void test_foreign_and_unaligned(void)
{
    static uint8_t bitmap[2];
    const wubu_pmm_region r = { 0x8000, 2 };
    wubu_pmm p;
    CHECK(wubu_pmm_init(&p, &r, 1, bitmap, sizeof bitmap) == 0, "init one region");

    wubu_pmm_free(&p, 0x9999);
    wubu_pmm_free(&p, 0x8001);
    CHECK(wubu_pmm_free_count(&p) == 2, "foreign/unaligned free changes nothing");
    CHECK(wubu_pmm_is_allocated(&p, 0x9999) == -1, "foreign frame reports unknown");

    CHECK(wubu_pmm_alloc(&p) == 0x8000, "first frame is the region base");
    CHECK(wubu_pmm_alloc(&p) == 0x9000, "second frame is one page later");
    CHECK(wubu_pmm_alloc(&p) == 0, "third alloc refused when full");
}

/* An unaligned region head must be dropped, not misreported as a frame. */
static void test_unaligned_normalisation(void)
{
    static uint8_t bitmap[4];
    const wubu_pmm_region r = { 0x1abc, 3 };   /* spans 0x1abc .. 0x3abc */
    wubu_pmm p;
    CHECK(wubu_pmm_init(&p, &r, 1, bitmap, sizeof bitmap) == 0, "init unaligned");
    CHECK(p.regions[0].base == 0x2000, "base rounded up to a frame");
    CHECK(p.regions[0].pages == 2 && wubu_pmm_total(&p) == 2,
          "only whole frames counted");

    CHECK(wubu_pmm_alloc(&p) == 0x2000, "first handed-out frame is 0x2000");
    CHECK(wubu_pmm_alloc(&p) == 0x3000, "last handed-out frame is 0x3000");
    CHECK(wubu_pmm_alloc(&p) == 0, "partial tail frame is never handed out");
}

static void test_bad_args(void)
{
    static uint8_t bitmap[4];
    const wubu_pmm_region r = { 0x1000, 1 };
    wubu_pmm p;
    CHECK(wubu_pmm_init(NULL, &r, 1, bitmap, sizeof bitmap) != 0, "NULL state refused");
    CHECK(wubu_pmm_init(&p, NULL, 1, bitmap, sizeof bitmap) != 0, "NULL regions refused");
    CHECK(wubu_pmm_init(&p, &r, 0, bitmap, sizeof bitmap) != 0, "zero regions refused");
    CHECK(wubu_pmm_init(&p, &r, 1, bitmap, 0) != 0, "bitmap too small refused");
    CHECK(wubu_pmm_alloc(NULL) == 0, "alloc on NULL is 0");
    CHECK(wubu_pmm_total(NULL) == 0, "total on NULL is 0");
    wubu_pmm_free(NULL, 0x1000);                 /* must not fault */
    CHECK(1, "free on NULL did not fault");
}

int main(void)
{
    test_two_region_index_space();
    test_foreign_and_unaligned();
    test_unaligned_normalisation();
    test_bad_args();
    if (fails) {
        printf("=== PMM tests: %d failed ===\n", fails);
        return 1;
    }
    printf("=== PMM tests: all passed ===\n");
    return 0;
}
