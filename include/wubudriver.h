#ifndef WUBU_DRIVER_H
#define WUBU_DRIVER_H

/*
 * WuBuDriver -- the kernel driver model.
 *
 * One concern: match devices to drivers and give each side a lifecycle hook.
 * No I/O, no locking policy beyond a single optional spinlock owner, no
 * allocation outside what the core pool provides.  Freestanding C11.
 */

typedef struct wubu_device wubu_device;
typedef struct wubu_driver wubu_driver;

/* Device class codes.  A device binds only to a driver of the same class. */
#define WUBU_CLASS_UART   0x55415254u   /* "UART" */
#define WUBU_CLASS_BLOCK  0x424C4B31u   /* "BLK1" */
#define WUBU_CLASS_INPUT  0x494E5054u   /* "INPT" */
#define WUBU_CLASS_GPU    0x47505531u   /* "GPU1" */
#define WUBU_CLASS_ANY    0xFFFFFFFFu   /* wildcard driver */

enum {
    WUBU_DEV_IDLE = 0,
    WUBU_DEV_BOUND,
    WUBU_DEV_FAILED
};

struct wubu_device {
    const char     *name;
    unsigned        class_id;
    void           *hw;             /* MMIO base / controller state */
    unsigned long   irq;
    int             state;
    wubu_driver    *driver;
    int             refcount;       /* open handles */
    wubu_device    *next;
};

struct wubu_driver {
    const char     *name;
    unsigned        class_id;       /* may be WUBU_CLASS_ANY */
    int             (*probe)(wubu_device *dev);      /* 0 = claimed */
    void            (*detach)(wubu_device *dev);
    void            (*shutdown)(wubu_device *dev);
    int             (*suspend)(wubu_device *dev);
    int             (*resume)(wubu_device *dev);
    wubu_driver    *next;
};

/* Registration.  Safe to call before or after wubu_bus_bind(). */
int  wubu_driver_register(wubu_driver *drv);      /* 0 ok, -1 full/dup */
int  wubu_driver_unregister(wubu_driver *drv);
int  wubu_device_register(wubu_device *dev);
int  wubu_device_unregister(wubu_device *dev);

/* Match every unbound device against registered drivers, in registration
 * order.  Returns the number of newly bound devices. */
int  wubu_bus_bind(void);
void wubu_bus_detach_all(void);

/* Lifecycle fan-out to every bound device. */
void wubu_bus_shutdown(void);
int  wubu_bus_suspend(void);
int  wubu_bus_resume(void);

int  wubu_device_open(wubu_device *dev);          /* refcount++ */
int  wubu_device_release(wubu_device *dev);       /* refcount--, detach at 0 */

int  wubu_driver_count(void);
int  wubu_device_count(void);
const wubu_device *wubu_device_first(void);

#endif /* WUBU_DRIVER_H */
