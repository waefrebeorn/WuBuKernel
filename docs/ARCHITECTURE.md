# Architecture

## Layers

```
WuBuOS (hosted + bare-metal OS)
   |
   |  src/kernel/WuBuKernel (submodule)
   v
WuBuKernel  -- core services + WuBuDriver
```

`wubukcore` owns process-independent services: console, heap, tick, panic.
`wubudriver` owns the device/driver registry and lifecycle. Neither layer
allocates outside the arena handed to `wubu_heap_init`.

## Binding contract

1. Drivers register first (registration order is binding priority).
2. Devices register with a class id.
3. `wubu_bus_bind()` walks devices in registration order and offers each to
   drivers of the same class (or `WUBU_CLASS_ANY`). First probe returning 0
   claims the device.
4. Unclaimed devices stay `IDLE`, so late-arriving drivers can be bound with
   another call.

Determinism matters: given identical registration order, binding is identical.

## Refcount rule

`wubu_device_open` requires a bound device. `release` drops the count. Detach is
driven by the bus, never by a consumer.

## Failure reporting

Services return 0 on success and -1 on refusal (full table, duplicate entry,
oversized allocation). Unrecoverable faults go through `wubu_panic`, which
records the reason, logs it, and halts.
