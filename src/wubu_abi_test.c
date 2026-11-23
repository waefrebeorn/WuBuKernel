/* wubu_abi_test.c -- RED-controlled gate for the versioned handle ABI. */
#include "wubu_abi.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    uint8_t wire[sizeof(wubu_abi_desc_t)];
    uint8_t bad[sizeof(wubu_abi_desc_t) + 8];
    wubu_abi_desc_t in = {
        WUBU_ABI_MAGIC, sizeof(wubu_abi_desc_t), WUBU_ABI_VERSION,
        WUBU_ABI_FLAG_PRESENT, 0x1122334455667788ull,
        0x8877665544332211ull, 0x0F000000ull
    };
    wubu_abi_desc_t out;

    printf("wubu_abi tests (v%u, %u bytes)\n",
           WUBU_ABI_VERSION, (unsigned)sizeof(wubu_abi_desc_t));
    CHECK(wubu_abi_encode(&in, wire, sizeof wire) == 0, "encode valid");
    CHECK(wubu_abi_decode(wire, sizeof wire, &out) == 0, "decode valid");
    CHECK(out.device_handle == in.device_handle, "device handle round-trip");
    CHECK(out.driver_handle == in.driver_handle, "driver handle round-trip");
    CHECK(out.mmio_token == in.mmio_token, "mmio token round-trip");

    CHECK(wubu_abi_decode(wire, sizeof wire - 1, &out) != 0,
          "truncated descriptor rejected");
    for (unsigned i = 0; i < sizeof bad; i++) bad[i] = wire[i % sizeof wire];

    bad[0] ^= 0x01;
    CHECK(wubu_abi_decode(bad, sizeof wire, &out) != 0, "bad magic rejected");
    bad[0] ^= 0x01;

    bad[8] = 3;
    CHECK(wubu_abi_decode(bad, sizeof wire, &out) != 0,
          "unknown version rejected");
    bad[8] = (uint8_t)WUBU_ABI_VERSION;

    bad[10] = 0x02;
    CHECK(wubu_abi_decode(bad, sizeof wire, &out) != 0,
          "reserved flag rejected");
    bad[10] = 0;

    bad[4] = 0;
    CHECK(wubu_abi_decode(bad, sizeof wire, &out) != 0,
          "wrong struct_size rejected");
    bad[4] = (uint8_t)sizeof(wubu_abi_desc_t);

    CHECK(wubu_abi_encode(&in, wire, sizeof wire - 1) != 0,
          "insufficient encode buffer rejected");
    CHECK(wubu_abi_decode(NULL, sizeof wire, &out) != 0, "null input rejected");

    if (fails == 0) printf("wubu_abi: ALL PASS\n");
    else printf("wubu_abi: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
