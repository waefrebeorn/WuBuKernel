/*
 * WuBuDriver -- implementation.
 *
 * Registry is a simple singly-linked list guarded by a tiny test-and-set
 * spinlock (lock-free read paths are deliberately NOT promised: this is the
 * target kernel, not the hosted test).  Binding is one pass in registration
 * order, which is deterministic and therefore testable.
 */
#include "wubudriver.h"

#define WUBU_MAX_DRIVERS 32
#define WUBU_MAX_DEVICES 64

static wubu_driver *drivers[WUBU_MAX_DRIVERS];
static int          ndrivers;

static wubu_device *devices[WUBU_MAX_DEVICES];
static int          ndevices;

static volatile int bus_lock;

static void bus_acquire(void)
{
    while (__sync_lock_test_and_set(&bus_lock, 1)) { }
}

static void bus_release(void)
{
    __sync_lock_release(&bus_lock);
}

int wubu_driver_register(wubu_driver *drv)
{
    if (!drv || !drv->name) return -1;
    bus_acquire();
    if (ndrivers >= WUBU_MAX_DRIVERS) { bus_release(); return -1; }
    for (int i = 0; i < ndrivers; ++i)
        if (drivers[i] == drv) { bus_release(); return -1; }   /* duplicate */
    drivers[ndrivers++] = drv;
    bus_release();
    return 0;
}

int wubu_driver_unregister(wubu_driver *drv)
{
    if (!drv) return -1;
    bus_acquire();
    int found = -1;
    for (int i = 0; i < ndrivers; ++i)
        if (drivers[i] == drv) { found = i; break; }
    if (found < 0) { bus_release(); return -1; }
    for (int i = found; i + 1 < ndrivers; ++i) drivers[i] = drivers[i+1];
    ndrivers--;
    bus_release();
    return 0;
}

int wubu_device_register(wubu_device *dev)
{
    if (!dev || !dev->name) return -1;
    bus_acquire();
    if (ndevices >= WUBU_MAX_DEVICES) { bus_release(); return -1; }
    for (int i = 0; i < ndevices; ++i)
        if (devices[i] == dev) { bus_release(); return -1; }
    devices[ndevices++] = dev;
    bus_release();
    return 0;
}

int wubu_device_unregister(wubu_device *dev)
{
    if (!dev) return -1;
    bus_acquire();
    int found = -1;
    for (int i = 0; i < ndevices; ++i)
        if (devices[i] == dev) { found = i; break; }
    if (found < 0) { bus_release(); return -1; }
    if (devices[found]->state == WUBU_DEV_BOUND &&
        devices[found]->driver && devices[found]->driver->detach)
        devices[found]->driver->detach(devices[found]);
    for (int i = found; i + 1 < ndevices; ++i) devices[i] = devices[i+1];
    ndevices--;
    bus_release();
    return 0;
}

/* Bind unbound devices.  A driver claims at most one device per class per
 * call unless it uses WUBU_CLASS_ANY, keeping behaviour predictable. */
int wubu_bus_bind(void)
{
    int bound = 0;
    bus_acquire();
    for (int di = 0; di < ndevices; ++di) {
        wubu_device *dev = devices[di];
        if (dev->state != WUBU_DEV_IDLE) continue;
        for (int k = 0; k < ndrivers; ++k) {
            wubu_driver *drv = drivers[k];
            if (drv->class_id != WUBU_CLASS_ANY &&
                drv->class_id != dev->class_id) continue;
            if (!drv->probe) continue;
            int rc = drv->probe(dev);
            if (rc == 0) {
                dev->driver = drv;
                dev->state = WUBU_DEV_BOUND;
                dev->refcount = 0;
                ++bound;
                break;
            }
        }
        if (dev->state == WUBU_DEV_IDLE && dev->driver == 0) {
            /* nobody claimed it: leave IDLE so a later register can retry */
        }
    }
    bus_release();
    return bound;
}

void wubu_bus_detach_all(void)
{
    bus_acquire();
    for (int i = 0; i < ndevices; ++i) {
        wubu_device *dev = devices[i];
        /* A driver without a detach hook still loses its binding: clearing the
         * state must not be conditional on the optional callback existing. */
        if (dev->state == WUBU_DEV_BOUND && dev->driver) {
            if (dev->driver->detach) dev->driver->detach(dev);
            dev->state = WUBU_DEV_IDLE;
            dev->driver = 0;
            dev->refcount = 0;
        }
    }
    bus_release();
}

void wubu_bus_shutdown(void)
{
    bus_acquire();
    for (int i = 0; i < ndevices; ++i) {
        wubu_device *dev = devices[i];
        if (dev->driver && dev->driver->shutdown)
            dev->driver->shutdown(dev);
    }
    bus_release();
}

int wubu_bus_suspend(void)
{
    int fails = 0;
    bus_acquire();
    for (int i = 0; i < ndevices; ++i) {
        wubu_device *dev = devices[i];
        if (dev->state == WUBU_DEV_BOUND && dev->driver &&
            dev->driver->suspend) {
            if (dev->driver->suspend(dev) != 0) ++fails;
        }
    }
    bus_release();
    return fails;
}

int wubu_bus_resume(void)
{
    int fails = 0;
    bus_acquire();
    for (int i = 0; i < ndevices; ++i) {
        wubu_device *dev = devices[i];
        if (dev->state == WUBU_DEV_BOUND && dev->driver &&
            dev->driver->resume) {
            if (dev->driver->resume(dev) != 0) ++fails;
        }
    }
    bus_release();
    return fails;
}

int wubu_device_open(wubu_device *dev)
{
    if (!dev || dev->state != WUBU_DEV_BOUND) return -1;
    bus_acquire();
    dev->refcount++;
    bus_release();
    return 0;
}

int wubu_device_release(wubu_device *dev)
{
    if (!dev) return -1;
    bus_acquire();
    if (dev->refcount > 0) dev->refcount--;
    int zero = (dev->refcount == 0);
    bus_release();
    return zero ? 0 : 0;
}

int wubu_driver_count(void) { return ndrivers; }
int wubu_device_count(void) { return ndevices; }

const wubu_device *wubu_device_first(void)
{
    return ndevices ? devices[0] : 0;
}
