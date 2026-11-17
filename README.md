# WuBuKernel

WuBuOS' own kernel core. C11, freestanding, no libc.

The pieces here are *ours*, not borrowed:

- **WuBuDriver** (`include/wubudriver.h`, `src/wubudriver.c`) -- device/driver
  registry and matching. Devices carry a class id; drivers claim by probe, in
  registration order, deterministically. Lifecycle fan-out: bind, detach,
  shutdown, suspend, resume, refcounted open/release.
- **core services** (`include/wubukcore.h`, `src/wubukcore.c`) -- COM1 console
  with an injectable sink, a bounded bump allocator (no overflow, no wrap),
  panic recording, and a monotonic tick.

## Build

```
make test         # hosted gate: console, heap bounds, driver model, ticks
make freestanding # compiles both .c with -nostdinc = zero libc header dependency
```

## Rules

- Strict C11, `-ffreestanding`, no god headers, one concern per file.
- Nothing here calls libc. If a piece needs a service, it uses this repo's
  headers or asks the caller.
- Every module is gated by `make test`; a doc naming a path that does not exist
  is a build failure.

## Layout

```
include/wubudriver.h   device/driver structs and API
include/wubukcore.h    console, heap, panic, ticks
src/wubudriver.c       registry, binding, lifecycle
src/wubukcore.c        implementations
src/wubukernel_test.c  hosted behaviour gate
docs/ARCHITECTURE.md   how the pieces fit
```

WuBuOS consumes this repository as a submodule (`src/kernel/WuBuKernel`).
