#ifndef WUBU_CAP_H
#define WUBU_CAP_H

/* WuBuKernel capability tokens: opaque u64 handles, never pointers.
 *
 * A caller cannot open a device, map MMIO, acquire an IRQ or create a DMA
 * domain by merely reaching a struct. It must present a granted token for
 * the exact right and object. Revocation is immediate and fail-closed.
 *
 * This is a portable contract module. L21 is unchanged: no cap object is
 * linked into kernel.elf until a consumer bridge proves it.
 */
#include <stdint.h>

#define WUBU_CAP_ROOT        0x5742554341500001ull  /* 'WUBCAP' root */
#define WUBU_CAP_NONE        0ull

enum wubu_cap_class {
    WUBU_CAP_DEV_OPEN   = 1u,
    WUBU_CAP_MMIO_MAP   = 2u,
    WUBU_CAP_IRQ_ACQUIRE = 3u,
    WUBU_CAP_DMA_DOMAIN = 4u
};

#define WUBU_CAP_ERR_DENIED  (-1)
#define WUBU_CAP_ERR_FULL    (-2)
#define WUBU_CAP_ERR_ARG     (-3)

/* Grant a right from a valid parent. Only ROOT may grant in v1. */
int wubu_cap_grant(uint64_t parent, uint32_t class, uint64_t object,
                   uint64_t *out);

/* 1: right is held, 0: absent/revoked/wrong class or object. */
int wubu_cap_check(uint64_t cap, uint32_t class, uint64_t object);

/* Revoke a token. 0 on success, negative on unknown/revoked token. */
int wubu_cap_revoke(uint64_t cap);

/* Drop all runtime grants (ROOT survives). */
void wubu_cap_reset(void);

#endif /* WUBU_CAP_H */
