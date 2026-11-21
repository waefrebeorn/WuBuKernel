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
