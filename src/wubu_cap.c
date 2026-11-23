/* wubu_cap.c -- bounded capability registry. One concern: authority. */
#include "wubu_cap.h"

#define WUBU_CAP_SLOTS 32u

typedef struct {
    uint64_t token;
    uint64_t object;
    uint32_t generation;
    uint32_t class;
    int used;
} cap_slot_t;

static cap_slot_t g_slots[WUBU_CAP_SLOTS];
static uint32_t g_generation = 1u;

static int valid_class(uint32_t class)
{
    return class >= WUBU_CAP_DEV_OPEN && class <= WUBU_CAP_DMA_DOMAIN;
}

void wubu_cap_reset(void)
{
    for (unsigned i = 0; i < WUBU_CAP_SLOTS; i++) {
        g_slots[i].used = 0;
        g_slots[i].token = WUBU_CAP_NONE;
    }
    g_generation++;
}

int wubu_cap_grant(uint64_t parent, uint32_t class, uint64_t object,
                   uint64_t *out)
{
    if (!out || !valid_class(class) || parent != WUBU_CAP_ROOT)
        return WUBU_CAP_ERR_ARG;
    for (unsigned i = 0; i < WUBU_CAP_SLOTS; i++) {
        cap_slot_t *s = &g_slots[i];
        if (s->used) continue;
        s->used = 1;
        s->class = class;
        s->object = object;
        s->generation = g_generation;
        s->token = ((uint64_t)g_generation << 16) | (uint64_t)(i + 1u);
        *out = s->token;
        return 0;
    }
    return WUBU_CAP_ERR_FULL;
}

int wubu_cap_check(uint64_t cap, uint32_t class, uint64_t object)
{
    if (cap == WUBU_CAP_NONE || !valid_class(class)) return 0;
    unsigned i = (unsigned)(cap & 0xFFFFu);
    if (i == 0 || i > WUBU_CAP_SLOTS) return 0;
    cap_slot_t *s = &g_slots[i - 1u];
    if (!s->used || s->token != cap) return 0;
    if (s->generation != (uint32_t)(cap >> 16)) return 0;
    return (s->class == class && s->object == object) ? 1 : 0;
}

int wubu_cap_revoke(uint64_t cap)
{
    if (cap == WUBU_CAP_NONE) return WUBU_CAP_ERR_ARG;
    unsigned i = (unsigned)(cap & 0xFFFFu);
    if (i == 0 || i > WUBU_CAP_SLOTS) return WUBU_CAP_ERR_DENIED;
    cap_slot_t *s = &g_slots[i - 1u];
    if (!s->used || s->token != cap) return WUBU_CAP_ERR_DENIED;
    s->used = 0;
    s->token = WUBU_CAP_NONE;
    g_generation++;
    return 0;
}
