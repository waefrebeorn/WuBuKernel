#ifndef WUBU_ABI_H
#define WUBU_ABI_H

/* Versioned, handle-only ABI descriptor for WuBuKernel consumers.
 *
 * No guest-visible field may carry a host pointer or function pointer:
 * handles are u64 tokens in the consumer's own namespace. WuBuKernel owns
 * the encoding; both driver registries may bind to it without sharing
 * internal struct layouts. Unknown/truncated descriptors FAIL CLOSED.
 */
#include <stdint.h>
#include <stddef.h>

#define WUBU_ABI_MAGIC      0x57425541u  /* 'WUBA' */
#define WUBU_ABI_VERSION    2u
#define WUBU_ABI_FLAG_PRESENT 0x1u

typedef struct {
    uint32_t magic;          /* WUBU_ABI_MAGIC */
    uint32_t struct_size;    /* sizeof(wubu_abi_desc_t) */
    uint16_t version;        /* WUBU_ABI_VERSION */
    uint16_t flags;          /* named bits only */
    uint64_t device_handle;  /* opaque, consumer namespace */
    uint64_t driver_handle;  /* opaque, consumer namespace */
    uint64_t mmio_token;     /* opaque mapping capability */
} wubu_abi_desc_t;

_Static_assert(sizeof(wubu_abi_desc_t) == 40, "ABI v2 descriptor layout drift");
_Static_assert(offsetof(wubu_abi_desc_t, device_handle) == 16,
               "handle fields must start at byte 8");

/* Encode into caller storage. Returns 0 on success, -1 on bad args. */
int wubu_abi_encode(const wubu_abi_desc_t *desc, void *buf, size_t cap);

/* Validate and decode. -1: truncated/unknown version/bad magic/flags. */
int wubu_abi_decode(const void *buf, size_t len, wubu_abi_desc_t *out);

#endif /* WUBU_ABI_H */
