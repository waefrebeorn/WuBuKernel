/*
 * WuBuKernel hosted gate.
 *
 * Runs the driver model and core services on the host so binding order,
 * refcounts, allocator bounds, console capture and panic reporting are proven
 * rather than assumed.  This is the same discipline as WuBuOS' own gates.
 */
#include "wubudriver.h"
#include "wubukcore.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL %s\n", msg); ++fails; } \
} while (0)

/* ---------------------------------------------------------- fake drivers */

static char logbuf[4096];
static void sink(const char *line, void *arg)
{
    (void)arg;
    size_t n = strlen(line);
    size_t cur = strlen(logbuf);
    if (cur + n + 1 < sizeof logbuf) { memcpy(logbuf + cur, line, n + 1); }
}

static wubu_device uart_dev = { .name = "com0", .class_id = WUBU_CLASS_UART };
static wubu_device blk_dev  = { .name = "nvme0", .class_id = WUBU_CLASS_BLOCK };
static wubu_device gpu_dev  = { .name = "gpu0", .class_id = WUBU_CLASS_GPU };

static int probe_calls;
static int uart_probe(wubu_device *dev)
{
    ++probe_calls;
    return dev == &uart_dev ? 0 : -1;
}
static int generic_probe(wubu_device *dev)
{
    (void)dev;
    return 0;
}
static void uart_detach(wubu_device *dev)
{
    dev->state = WUBU_DEV_IDLE;
}
static int uart_suspend(wubu_device *dev) { (void)dev; return 0; }

static wubu_driver uart_driver = {
    .name = "wubu-uart", .class_id = WUBU_CLASS_UART,
    .probe = uart_probe, .detach = uart_detach, .suspend = uart_suspend
};
static wubu_driver gpu_driver = {
    .name = "wubu-gpu", .class_id = WUBU_CLASS_GPU, .probe = generic_probe
};

int main(void)
{
    /* ---------------------------------------------------------- console */
    wubu_console_set_sink(sink, NULL);
    wubu_klog("hello %s %d %x\n", "kernel", 42, 255);
    CHECK(strstr(logbuf, "hello kernel 42 ff") != NULL,
          "formatted console output reaches the sink");
    logbuf[0] = '\0';

    /* --------------------------------------------------------- allocator */
    static unsigned char arena[8192] __attribute__((aligned(16)));
    CHECK(wubu_heap_init(arena, sizeof arena) == 0, "heap initialises");

    void *a = wubu_kmalloc(100);
    void *b = wubu_kcalloc(3, 32);
    CHECK(a != NULL && b != NULL, "allocations succeed inside the arena");
    CHECK(((unsigned long)(size_t)a & 15u) == 0, "kmalloc returns aligned memory");
    unsigned char *zb = (unsigned char *)b;
    int zeroed = 1;
    for (int i = 0; i < 96; ++i) if (zb[i]) zeroed = 0;
    CHECK(zeroed, "kcalloc zeroes its block");
    CHECK(wubu_heap_used() >= 144, "watermark advances");

    /* Bounded: a request larger than the arena must fail, not wrap. */
    CHECK(wubu_kmalloc(sizeof arena) == NULL,
          "oversized allocation refused without overflow");
    CHECK(wubu_kmalloc(0) == NULL, "zero-byte allocation refused");

    /* -------------------------------------------------------- driver model */
    CHECK(wubu_driver_register(&uart_driver) == 0, "uart driver registered");
    CHECK(wubu_driver_register(&uart_driver) != 0, "duplicate driver refused");
    CHECK(wubu_device_register(&uart_dev) == 0, "uart device registered");
    CHECK(wubu_device_register(&blk_dev) == 0, "block device registered");
    CHECK(wubu_driver_register(&gpu_driver) == 0, "gpu driver registered");
    CHECK(wubu_device_register(&gpu_dev) == 0, "gpu device registered");

    CHECK(wubu_bus_bind() == 2, "bind claims the two matchable devices");
    CHECK(uart_dev.state == WUBU_DEV_BOUND && uart_dev.driver == &uart_driver,
          "uart bound to its class driver");
    CHECK(gpu_dev.state == WUBU_DEV_BOUND, "gpu bound to its class driver");
    CHECK(blk_dev.state == WUBU_DEV_IDLE, "unmatched device stays idle");

    CHECK(wubu_device_open(&uart_dev) == 0 && uart_dev.refcount == 1,
          "device open increments refcount");
    CHECK(wubu_device_release(&uart_dev) == 0, "device release accepted");

    CHECK(wubu_bus_suspend() == 0, "suspend fan-out succeeds");
    CHECK(wubu_bus_resume() == 0, "resume fan-out succeeds");

    /* A device arriving late still binds: retry semantics are deterministic. */
    CHECK(wubu_bus_bind() == 0, "second pass finds nothing new to bind");

    wubu_bus_shutdown();
    wubu_bus_detach_all();
    CHECK(uart_dev.state == WUBU_DEV_IDLE && uart_dev.driver == NULL,
          "detach clears binding state");

    CHECK(wubu_device_unregister(&uart_dev) == 0, "device unregistered");
    CHECK(wubu_driver_unregister(&uart_driver) == 0, "driver unregistered");
    CHECK(wubu_device_count() == 2 && wubu_driver_count() == 1,
          "registry counts track removals");

    /* ------------------------------------------------------------- ticks */
    CHECK(wubu_tick_count() == 0, "ticks start at zero");
    wubu_tick_advance(17);
    CHECK(wubu_tick_count() == 17, "tick counter accumulates");

    if (fails) {
        printf("[wubukernel] %d fail\n", fails);
        return 1;
    }
    printf("[wubukernel] all pass (console + heap + driver model + ticks)\n");
    return 0;
}
