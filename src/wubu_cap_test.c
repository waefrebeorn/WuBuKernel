/* wubu_cap_test.c -- RED-controlled authority gate for capability tokens. */
#include "wubu_cap.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    uint64_t dev = WUBU_CAP_NONE, mmio = WUBU_CAP_NONE;
    printf("wubu_cap tests (root=%llx)\n",
           (unsigned long long)WUBU_CAP_ROOT);
    wubu_cap_reset();

    CHECK(wubu_cap_check(WUBU_CAP_NONE, WUBU_CAP_DEV_OPEN, 1) == 0,
          "no token is denied");
    CHECK(wubu_cap_check(WUBU_CAP_ROOT, WUBU_CAP_DEV_OPEN, 1) == 0,
          "root is not a device token");
    CHECK(wubu_cap_grant(WUBU_CAP_NONE, WUBU_CAP_DEV_OPEN, 1, &dev) < 0,
          "non-root parent cannot grant");
    CHECK(dev == WUBU_CAP_NONE, "failed grant emits no token");

    CHECK(wubu_cap_grant(WUBU_CAP_ROOT, WUBU_CAP_DEV_OPEN, 1, &dev) == 0,
          "root grants DEV_OPEN");
    CHECK(dev != WUBU_CAP_NONE, "granted token is non-zero");
    CHECK(wubu_cap_check(dev, WUBU_CAP_DEV_OPEN, 1) == 1,
          "granted right is held");
    CHECK(wubu_cap_check(dev, WUBU_CAP_DEV_OPEN, 2) == 0,
          "wrong object is denied");
    CHECK(wubu_cap_check(dev, WUBU_CAP_MMIO_MAP, 1) == 0,
          "wrong class is denied");
    CHECK(wubu_cap_grant(dev, WUBU_CAP_MMIO_MAP, 1, &mmio) < 0,
          "non-root token cannot grant in v1");

    CHECK(wubu_cap_grant(WUBU_CAP_ROOT, WUBU_CAP_MMIO_MAP, 0x1000,
                         &mmio) == 0, "root grants MMIO_MAP");
    CHECK(wubu_cap_check(mmio, WUBU_CAP_MMIO_MAP, 0x1000) == 1,
          "mmio right is held");

    CHECK(wubu_cap_revoke(dev) == 0, "revoke succeeds");
    CHECK(wubu_cap_check(dev, WUBU_CAP_DEV_OPEN, 1) == 0,
          "revoked right fails closed");
    CHECK(wubu_cap_revoke(dev) < 0, "double revoke is refused");
    CHECK(wubu_cap_check(mmio, WUBU_CAP_MMIO_MAP, 0x1000) == 1,
          "revoke of one token does not leak to another");

    CHECK(wubu_cap_grant(WUBU_CAP_ROOT, 99u, 1, &dev) < 0,
          "unknown class is refused");
    wubu_cap_reset();
    CHECK(wubu_cap_check(mmio, WUBU_CAP_MMIO_MAP, 0x1000) == 0,
          "reset drops runtime grants");

    if (fails == 0) printf("wubu_cap: ALL PASS\n");
    else printf("wubu_cap: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
