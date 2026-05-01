# scheduler

**STATUS: candidate**
**Layer: L3 (modules)**

Cooperative task scheduler for bare-metal and single-threaded environments.
Round-robin periodic task execution, tick-driven. Application calls
`scheduler_tick()` + `scheduler_run()` in main loop.

Passive design: no OS, no malloc, no timers, no interrupts. All timing
expressed in tick counts -- the caller decides the tick rate.

## Why not VSF native?

Checked VSF source (2026-05-01): VSF has `vsf_eda_t` and `vsf_teda_t`
(timed event-driven architecture) which provide cooperative scheduling, but
they require the full VSF kernel and event infrastructure. No standalone
tick-driven round-robin scheduler usable as a passive library without OS
dependencies.

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

## Test status

- Unit tests: **20 C cases** -- init, init null, add task, add null params,
  run at interval, run before interval (skip), multiple tasks different
  intervals, enable/disable, enable/disable not found, remove, remove not
  found, max tasks, run count, run count not found, task context, zero
  interval (every tick), overrun detection, find by name, tick/run null
  safety, remove middle preserves order

## Dependencies

None.

## How to test

```bash
cd wheels/modules/scheduler/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Origin

Created 2026-05-01 for cooperative task scheduling in bare-metal main loops.

## Known issues

None.

## Supported platforms

- [x] Linux host (sim)

## API review

- Proposal date: 2026-05-01
- Reviewer: @user (pending review)

## API convention deviations

None.

## Responsible human

@user
