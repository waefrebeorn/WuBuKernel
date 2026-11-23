#ifndef WUBU_DRIVER_CONTRACT_H
#define WUBU_DRIVER_CONTRACT_H

/*
 * WuBu driver contract -- the BAR/MMIO invariants shared by BOTH driver
 * registries (WuBuKernel's wubudriver and WuBuOS's wubu_drv).
 *
 * This is a CONTRACT, not an adoption: L21 still stands -- WuBuKernel
 * objects are NOT linked into the running kernel. Both sides honour the
 * same encoding and lifecycle, proved by the bridge gate.
 *
 * Lifecycle (ordering matters):
 *   discover BAR (config space) -> bind driver -> map BAR AFTER vmm init
 *   -> driver dereferences the returned VA, never the raw physical BAR.
 * A zero VA means "no mapping". Absent hardware is absent (-1 / NONE),
 * never a fabricated zero value.
 */
#include <stdint.h>

#define WUBU_DRIVER_CONTRACT_VERSION 1u

/* PCI BAR encoding. Bit 0 SET is I/O space; CLEAR is memory space. */
#define WUBU_CONTRACT_BAR_MEM        0u
#define WUBU_CONTRACT_BAR_IO         1u
#define WUBU_CONTRACT_BAR_TYPE_MASK  0xFull
#define WUBU_CONTRACT_BAR_64BIT      0x4u    /* bits [3:2] == 10 */
#define WUBU_CONTRACT_BAR_TYPE64_MASK 0x6u   /* the two type bits [3:2] */

/* MMIO page flags: present | writable (the wubu_vmm_map_page encoding). */
#define WUBU_CONTRACT_MAP_FLAGS      3u

/* Named MMIO flags for v2. Do not pass the integer 3 across the ABI. */
#define WUBU_CONTRACT_FLAG_PRESENT        0x1u
#define WUBU_CONTRACT_FLAG_WRITABLE       0x2u
#define WUBU_CONTRACT_FLAG_CACHE_INHIBIT  0x4u

/* BAR type encodings from bits [3:2] after the space bit is stripped. */
#define WUBU_CONTRACT_BARTYPE_32BIT       0u
#define WUBU_CONTRACT_BARTYPE_1MB         1u  /* legacy, rejected */
#define WUBU_CONTRACT_BARTYPE_64BIT       2u  /* bits [3:2] == 10 */
#define WUBU_CONTRACT_BARTYPE_RESERVED    3u  /* rejected */

typedef struct {
    uint64_t phys;               /* 64-bit base, type bits stripped */
    uint64_t size;               /* aperture size from all-ones probe */
    uint32_t type;               /* WUBU_CONTRACT_BARTYPE_* */
    uint32_t flags;              /* named WUBU_CONTRACT_FLAG_* bits */
} wubu_contract_bar_v2_t;

/* Sentinel: no mapping established. */
#define WUBU_CONTRACT_VA_UNMAPPED    0ull

enum wubu_contract_mmio_state {
    WUBU_CONTRACT_MMIO_NONE = 0,
    WUBU_CONTRACT_MMIO_UNMAPPED,
    WUBU_CONTRACT_MMIO_MAPPED
};

/* Decode a device's BAR0 (+BAR1 high dword for a 64-bit BAR) into a
 * physical page address. Returns 0 and writes *phys on success; -1 for
 * no BAR, an I/O-space BAR, or a null out pointer. */
int wubu_contract_decode_bar(uint64_t bar0, uint64_t bar1, uint64_t *phys);

/* v2: decode base + size from the all-ones probe readback. bar1_mask is
 * the high dword probe for a 64-bit BAR. 0 on success, -1 on I/O BAR,
 * reserved/legacy type, zero size, overflow, or null out pointer.
 * The probe value has bit0 clear for memory space (see above). */
int wubu_contract_decode_bar_v2(uint32_t bar0, uint32_t bar1,
                                uint32_t bar0_probe, uint32_t bar1_probe,
                                wubu_contract_bar_v2_t *out);

/* 1 when the BAR describes a memory-space MMIO window. */
int wubu_contract_is_mmio(uint64_t bar0);

/* Compile-time invariants: drift breaks the build, not the boot. */
_Static_assert(WUBU_CONTRACT_BAR_MEM == 0, "memory BAR bit must be clear");
_Static_assert(WUBU_CONTRACT_BAR_IO == 1, "I/O BAR bit must be set");
_Static_assert((WUBU_CONTRACT_BAR_TYPE_MASK & WUBU_CONTRACT_BAR_IO) != 0,
               "BAR type mask must include the space bit");
_Static_assert((WUBU_CONTRACT_BAR_64BIT & WUBU_CONTRACT_BAR_IO) == 0,
               "64-bit type bits must not include the space bit");
_Static_assert(WUBU_CONTRACT_MAP_FLAGS == 3, "MMIO must be present+RW");
_Static_assert(WUBU_CONTRACT_VA_UNMAPPED == 0, "unmapped sentinel must be 0");

#endif /* WUBU_DRIVER_CONTRACT_H */
