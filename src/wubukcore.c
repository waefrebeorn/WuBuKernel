/*
 * WuBuKernel core services.
 *
 * Freestanding: no libc.  The allocator is a bounded bump with watermark, the
 * console writes bytes straight to COM1 (or to an injected sink under test),
 * and panic records the reason before halting.
 */
#include "wubukcore.h"

/* ------------------------------------------------------------ tiny string */

static size_t k_strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static void k_memset(void *dst, int v, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--) *p++ = (unsigned char)v;
}

static void k_memcpy(void *dst, const char *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)*src++;
}

/* --------------------------------------------------------------- console */

static unsigned short com_port = 0x3F8;
static wubu_log_sink sink_fn;
static void *sink_arg;

void wubu_console_init(unsigned short port) { com_port = port; }

void wubu_console_set_sink(wubu_log_sink sink, void *arg)
{
    sink_fn = sink;
    sink_arg = arg;
}

#if defined(__x86_64__)
static void outb(unsigned short port, unsigned char v)
{
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}
static unsigned char inb(unsigned short port)
{
    unsigned char v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
#else
static void outb(unsigned short p, unsigned char v) { (void)p; (void)v; }
static unsigned char inb(unsigned short p) { (void)p; return 0xFF; }
#endif

#define COM_LSR      5
#define COM_LSR_TXRDY 0x20

void wubu_klog_raw(const char *s)
{
    if (!s) return;
    if (sink_fn) {
        sink_fn(s, sink_arg);
        return;
    }
    size_t n = k_strlen(s);
    for (size_t i = 0; i < n; ++i) {
        while (!(inb(com_port + COM_LSR) & COM_LSR_TXRDY)) { }
        outb(com_port, (unsigned char)s[i]);
    }
}

/* Minimal formatter: supports %s %c %d %u %x %p %%.  No floats, no width
 * games -- kernel messages must be predictable. */
static void emit_u(unsigned long v, int base, char *tmp, int *n)
{
    const char *dig = "0123456789abcdef";
    char rev[24];
    int r = 0;
    if (v == 0) rev[r++] = '0';
    while (v) { rev[r++] = dig[v % (unsigned)base]; v /= (unsigned)base; }
    while (r) tmp[(*n)++] = rev[--r];
}

void wubu_klog(const char *fmt, ...)
{
    char out[256];
    int o = 0;
    const char *p = fmt;

    /* Variadic access without stdarg would be impossible; we declare it through
     * the compiler builtin so this stays freestanding. */
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    while (*p && o < (int)sizeof out - 2) {
        if (*p != '%') { out[o++] = *p++; continue; }
        char c = *++p;
        if (c == '%') { out[o++] = '%'; p++; continue; }
        if (c == 's') {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && o < (int)sizeof out - 2) out[o++] = *s++;
        } else if (c == 'c') {
            out[o++] = (char)__builtin_va_arg(ap, int);
        } else if (c == 'd' || c == 'i') {
            long v = __builtin_va_arg(ap, int);
            if (v < 0) { out[o++] = '-'; v = -v; }
            emit_u((unsigned long)v, 10, out, &o);
        } else if (c == 'u') {
            emit_u((unsigned long)__builtin_va_arg(ap, unsigned), 10, out, &o);
        } else if (c == 'x') {
            emit_u((unsigned long)__builtin_va_arg(ap, unsigned), 16, out, &o);
        } else if (c == 'p') {
            unsigned long v = (unsigned long)(size_t)__builtin_va_arg(ap, void *);
            out[o++] = '0'; out[o++] = 'x';
            emit_u(v, 16, out, &o);
        } else {
            out[o++] = '?';
        }
        p++;
    }
    __builtin_va_end(ap);
    out[o] = '\0';
    wubu_klog_raw(out);
}

/* -------------------------------------------------------------- allocator */

static unsigned char *heap_base;
static size_t         heap_size;
static size_t         heap_used;
static int            heap_ready;

int wubu_heap_init(void *base, size_t bytes)
{
    if (!base || !bytes) return -1;
    heap_base = (unsigned char *)base;
    heap_size = bytes;
    heap_used = 0;
    heap_ready = 1;
    return 0;
}

static size_t align16(size_t n)
{
    return (n + 15u) & ~(size_t)15u;
}

void *wubu_kmalloc(size_t bytes)
{
    if (!heap_ready || bytes == 0) return 0;
    size_t off = align16(heap_used);
    if (bytes > heap_size - off) return 0;   /* bounded: never overflow */
    heap_used = off + bytes;
    return heap_base + off;
}

void *wubu_kcalloc(size_t count, size_t bytes)
{
    size_t total = count * bytes;
    if (count && total / count != bytes) return 0; /* overflow guard */
    void *p = wubu_kmalloc(total);
    if (p) k_memset(p, 0, total);
    return p;
}

void wubu_kfree(void *p) { (void)p; }   /* bump semantics: reclaim via reset */

size_t wubu_heap_free(void)
{
    return heap_size - heap_used;
}

size_t wubu_heap_used(void) { return heap_used; }

/* ------------------------------------------------------------------ misc */

static char          panic_reason[64];
static unsigned long ticks;

void wubu_panic(const char *reason)
{
    size_t n = k_strlen(reason);
    if (n >= sizeof panic_reason) n = sizeof panic_reason - 1;
    k_memcpy(panic_reason, reason, n);
    panic_reason[n] = '\0';

    wubu_klog("PANIC: ");
    wubu_klog_raw(panic_reason);
    wubu_klog_raw("\n");

#if defined(__x86_64__)
    for (;;) __asm__ volatile ("cli; hlt");
#else
    for (;;) { }
#endif
}

const char *wubu_panic_reason(void)
{
    return panic_reason;
}

unsigned long wubu_tick_count(void) { return ticks; }

void wubu_tick_advance(unsigned long n) { ticks += n; }
