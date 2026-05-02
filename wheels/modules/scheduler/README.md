# scheduler

**Status: candidate**
**Layer: L3 (modules)**

Cooperative task scheduler for bare-metal and single-threaded environments.
Round-robin periodic task execution, tick-driven. Application calls
`scheduler_tick()` + `scheduler_run()` in main loop.

Passive design: no OS, no malloc, no timers, no interrupts. All timing
expressed in tick counts -- the caller decides the tick rate.

## Origin

Created 2026-05-01 for cooperative task scheduling in bare-metal main loops.

## Why self-written?

Searched GitHub (2026-05-02). Referenced bradschl/embedded-c-scheduler (MIT). Uses heap at init; our zero-malloc fits contract. Self-written for platform convention consistency.

## Dependencies

None.

## API

```c
sched_err_t scheduler_init(scheduler_t *s);
sched_err_t scheduler_add(scheduler_t *s, const char *name,
                          sched_task_fn fn, void *ctx,
                          uint32_t interval_ticks);
void        scheduler_tick(scheduler_t *s);
void        scheduler_run(scheduler_t *s);
sched_err_t scheduler_enable(scheduler_t *s, const char *name);
sched_err_t scheduler_disable(scheduler_t *s, const char *name);
sched_err_t scheduler_remove(scheduler_t *s, const char *name);
uint8_t     scheduler_count(const scheduler_t *s);
uint32_t    scheduler_get_run_count(const scheduler_t *s, const char *name);
```

### Error codes

| Code | Value | Meaning |
|------|-------|---------|
| `SCHED_OK` | 0 | Success |
| `SCHED_ERR_PARAMS` | -1 | NULL pointer or task not found |
| `SCHED_ERR_FULL` | -2 | Task array full (`SCHED_MAX_TASKS`) |

### Configuration

| Macro | Default | Purpose |
|-------|---------|---------|
| `SCHED_MAX_TASKS` | 16 | Max registered tasks (compile-time) |

### Overrun detection

Each task tracks `overrun_count` -- incremented when `scheduler_run()` finds
that more than one full interval has elapsed since the last execution. Useful
for detecting tasks that are starved or main loops that are too slow.

## Supported platforms

- [x] Linux host (sim)

## Known issues

None.

## How to test

```bash
cd wheels/modules/scheduler/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **20 C cases** -- init, init null, add task, add null params,
  run at interval, run before interval (skip), multiple tasks different
  intervals, enable/disable, enable/disable not found, remove, remove not
  found, max tasks, run count, run count not found, task context, zero
  interval (every tick), overrun detection, find by name, tick/run null
  safety, remove middle preserves order

## API convention deviations

None.

## API review

- Proposal date: 2026-05-01
- Reviewer: @user (pending review)

## Responsible human

@user
