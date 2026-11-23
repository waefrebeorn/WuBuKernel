/* wubu_driver_contract.c -- the shared BAR/MMIO contract implementation.
 * Freestanding C11; no libc, no I/O. Both registries call these decoders
 * so there is exactly one place that knows the PCI BAR encoding. */
#include "wubu_driver_contract.h"

int wubu_contract_is_mmio(uint64_t bar0)
{
    return bar0 != 0 && !(bar0 & WUBU_CONTRACT_BAR_IO);
}

int wubu_contract_decode_bar(uint64_t bar0, uint64_t bar1, uint64_t *phys)
{
    if (!phys || !wubu_contract_is_mmio(bar0)) return -1;
    uint64_t p = bar0 & ~WUBU_CONTRACT_BAR_TYPE_MASK;
    if ((bar0 & WUBU_CONTRACT_BAR_TYPE64_MASK) == WUBU_CONTRACT_BAR_64BIT)
        p |= bar1 << 32;          /* 64-bit memory BAR: BAR1 is the high dword */
    if (!p) return -1;
    *phys = p;
    return 0;
}

int wubu_contract_decode_bar_v2(uint32_t bar0, uint32_t bar1,
                                uint32_t bar0_probe, uint32_t bar1_probe,
                                wubu_contract_bar_v2_t *out)
{
    if (!out) return -1;
    if ((bar0 & WUBU_CONTRACT_BAR_IO) != 0 ||
        (bar0_probe & WUBU_CONTRACT_BAR_IO) != 0) return -1;

    uint32_t type = (bar0 >> 1) & 0x3u;
    uint32_t ptype = (bar0_probe >> 1) & 0x3u;
    if (type != ptype || type == WUBU_CONTRACT_BARTYPE_1MB ||
        type == WUBU_CONTRACT_BARTYPE_RESERVED) return -1;

    uint64_t mask64 = (uint64_t)(bar0_probe & 0xFFFFFFF0u);
    uint64_t phys = (uint64_t)(bar0 & 0xFFFFFFF0u);
    if (type == WUBU_CONTRACT_BARTYPE_64BIT) {
        /* A zero high-dword probe means the upper bits are not writable;
         * treat them as fixed so the aperture size stays in the low half. */
        mask64 |= bar1_probe ? ((uint64_t)bar1_probe << 32)
                             : 0xFFFFFFFF00000000ull;
        phys |= (uint64_t)bar1 << 32;
    } else {
        /* A 32-bit BAR must not gain a high dword from BAR1. */
        mask64 |= 0xFFFFFFFF00000000ull;
    }
    if (mask64 == 0xFFFFFFFFFFFFFFFFull) return -1; /* probe not implemented */
    uint64_t size = (~mask64) + 1ull;
    if (size == 0 || (size & (size - 1ull)) != 0) return -1; /* non power-of-two */
    if (phys == 0) return -1;

    out->phys = phys;
    out->size = size;
    out->type = type;
    out->flags = WUBU_CONTRACT_FLAG_PRESENT | WUBU_CONTRACT_FLAG_WRITABLE |
                 WUBU_CONTRACT_FLAG_CACHE_INHIBIT;
    return 0;
}
