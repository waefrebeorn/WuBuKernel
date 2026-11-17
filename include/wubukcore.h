#ifndef WUBUKCORE_H
#define WUBUKCORE_H

/*
 * WuBuKernel core services: the smallest set every kernel piece needs --
 * console output, a bounded allocator, panic, and a monotonic tick.
 *
 * Nothing here touches libc.  On the target these issue real port I/O; on a
 * host build the console can be redirected to a callback so the logic is
 * testable without hardware.
 */

#include <stddef.h>

/* -------------------------------------------------------------- console */
typedef void (*wubu_log_sink)(const char *line, void *arg);

void wubu_console_init(unsigned short port);       /* 0x3F8 COM1 */
void wubu_console_set_sink(wubu_log_sink sink, void *arg);
void wubu_klog(const char *fmt, ...);
void wubu_klog_raw(const char *s);

/* ------------------------------------------------------------- allocator */
int   wubu_heap_init(void *base, size_t bytes);
void *wubu_kmalloc(size_t bytes);        /* 16-byte aligned, NULL when full */
void *wubu_kcalloc(size_t count, size_t bytes);
void  wubu_kfree(void *p);               /* bump: no-op, kept for ABI symmetry */
size_t wubu_heap_free(void);
size_t wubu_heap_used(void);

/* ----------------------------------------------------------------- misc */
void     wubu_panic(const char *reason) __attribute__((noreturn));
const char *wubu_panic_reason(void);
unsigned long wubu_tick_count(void);
void     wubu_tick_advance(unsigned long n);

#endif /* WUBUKCORE_H */
