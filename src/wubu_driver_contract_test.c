/* wubu_driver_contract_test.c -- behaviour gate for the shared BAR/MMIO
 * contract. WuBuKernel owns the contract; both registries must agree. */
#include "wubu_driver_contract.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    uint64_t p = 0;
    printf("wubu_driver_contract tests (v%u)\n", WUBU_DRIVER_CONTRACT_VERSION);

    CHECK(WUBU_DRIVER_CONTRACT_VERSION == 1, "contract version");
    CHECK(WUBU_CONTRACT_MMIO_NONE == 0, "NONE state is zero");

    /* memory BAR: bit 0 clear, type bits stripped */
    CHECK(wubu_contract_is_mmio(0xFC000000ull) == 1, "memory BAR is MMIO");
    CHECK(wubu_contract_decode_bar(0xFC000000ull, 0, &p) == 0, "decode 32-bit BAR");
    CHECK(p == 0xFC000000ull, "32-bit BAR phys");

    /* I/O-space BAR: bit 0 set -- rejected, never mapped */
    CHECK(wubu_contract_is_mmio(0x1F0ull | WUBU_CONTRACT_BAR_IO) == 0,
          "I/O BAR is not MMIO");
    CHECK(wubu_contract_decode_bar(0x1F0ull | WUBU_CONTRACT_BAR_IO, 0, &p) != 0,
          "I/O BAR rejected");
    CHECK(wubu_contract_decode_bar(0, 0, &p) != 0, "no BAR rejected");
    CHECK(wubu_contract_decode_bar(0xFC000000ull, 0, NULL) != 0, "null out rejected");

    /* 64-bit memory BAR: type bits 10, BAR1 is the high dword */
    CHECK(wubu_contract_decode_bar(0xFC000004ull, 1, &p) == 0, "decode 64-bit BAR");
    CHECK(p == 0x1FC000000ull, "64-bit BAR phys assembled from BAR1");

    /* shared map flags: present + writable */
    CHECK(WUBU_CONTRACT_MAP_FLAGS == 3, "MMIO mapping is present+RW");
    CHECK(WUBU_CONTRACT_VA_UNMAPPED == 0, "unmapped VA sentinel is zero");

    /* v2: base + size + named flags, v1 wrapper still intact */
    wubu_contract_bar_v2_t v2;
    CHECK(wubu_contract_decode_bar_v2(0xFC000000u, 0, 0xFFF00000u, 0,
                                      &v2) == 0, "v2 decode 32-bit BAR");
    CHECK(v2.phys == 0xFC000000ull, "v2 base");
    CHECK(v2.size == 0x100000ull, "v2 size from all-ones probe");
    CHECK(v2.type == WUBU_CONTRACT_BARTYPE_32BIT, "v2 32-bit type");
    CHECK(v2.flags == (WUBU_CONTRACT_FLAG_PRESENT |
                       WUBU_CONTRACT_FLAG_WRITABLE |
                       WUBU_CONTRACT_FLAG_CACHE_INHIBIT), "v2 named flags");
    CHECK(wubu_contract_decode_bar_v2(0xFC000004u, 1, 0xFFF00004u, 0,
                                      &v2) == 0, "v2 decode 64-bit BAR");
    CHECK(v2.phys == 0x1FC000000ull, "v2 64-bit base from BAR1");
    CHECK(v2.size == 0x100000ull, "v2 64-bit size");
    CHECK(v2.type == WUBU_CONTRACT_BARTYPE_64BIT, "v2 64-bit type");
    CHECK(wubu_contract_decode_bar_v2(0xFC000002u, 0, 0xFFF00002u, 0,
                                      &v2) != 0, "v2 legacy 1MB BAR rejected");
    CHECK(wubu_contract_decode_bar_v2(0xFC000006u, 0, 0xFFF00006u, 0,
                                      &v2) != 0, "v2 reserved type rejected");
    CHECK(wubu_contract_decode_bar_v2(0x1F0u | WUBU_CONTRACT_BAR_IO, 0,
                                      0xFFFFFFF1u, 0, &v2) != 0,
          "v2 I/O BAR rejected");
    CHECK(wubu_contract_decode_bar_v2(0xFC000000u, 0, 0xFFF00000u, 0,
                                      NULL) != 0, "v2 null out rejected");

    if (fails == 0) printf("wubu_driver_contract: ALL PASS\n");
    else printf("wubu_driver_contract: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
