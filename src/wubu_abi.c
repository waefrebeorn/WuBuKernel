/* wubu_abi.c -- versioned handle-only descriptor. One concern: ABI wire
 * format. Internal pointer structs stay in the implementing module. */
#include "wubu_abi.h"

static void copy_bytes(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static void wr32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void wr64(uint8_t *p, uint64_t v)
{
    wr32(p, (uint32_t)v);
    wr32(p + 4, (uint32_t)(v >> 32));
}

int wubu_abi_encode(const wubu_abi_desc_t *desc, void *buf, size_t cap)
{
    if (!desc || !buf || cap < sizeof(wubu_abi_desc_t)) return -1;
    if (desc->magic != WUBU_ABI_MAGIC ||
        desc->struct_size != sizeof(wubu_abi_desc_t) ||
        desc->version != WUBU_ABI_VERSION ||
        (desc->flags & ~(uint16_t)WUBU_ABI_FLAG_PRESENT) != 0) return -1;
    uint8_t *p = (uint8_t *)buf;
    wr32(p + 0, desc->magic);
    wr32(p + 4, desc->struct_size);
    wr32(p + 8, (uint32_t)desc->version | ((uint32_t)desc->flags << 16));
    wr64(p + 16, desc->device_handle);
    wr64(p + 24, desc->driver_handle);
    wr64(p + 32, desc->mmio_token);
    return 0;
}

int wubu_abi_decode(const void *buf, size_t len, wubu_abi_desc_t *out)
{
    if (!buf || !out || len < sizeof(wubu_abi_desc_t)) return -1;
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t magic = rd32(p + 0);
    uint32_t size = rd32(p + 4);
    uint32_t vf = rd32(p + 8);
    uint16_t version = (uint16_t)vf;
    uint16_t flags = (uint16_t)(vf >> 16);
    if (magic != WUBU_ABI_MAGIC ||
        size != sizeof(wubu_abi_desc_t) ||
        version != WUBU_ABI_VERSION ||
        (flags & ~(uint16_t)WUBU_ABI_FLAG_PRESENT) != 0) return -1;
    wubu_abi_desc_t tmp;
    tmp.magic = magic;
    tmp.struct_size = size;
    tmp.version = version;
    tmp.flags = flags;
    tmp.device_handle = rd64(p + 16);
    tmp.driver_handle = rd64(p + 24);
    tmp.mmio_token = rd64(p + 32);
    copy_bytes(out, &tmp, sizeof(tmp));
    return 0;
}
