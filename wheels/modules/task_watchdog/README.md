# task_watchdog — passive tick-driven task health monitor

**Status: candidate**
**Layer: L3 (modules)**

Registers tasks with a timeout. Each task calls `feed()` periodically.
Application calls `tick()` to advance time and `check()` to detect expired
tasks. Callback fires on expiry. Fully passive — no timers, no interrupts,
no OS dependency.

## Origin

Proactive build for embedded systems needing software task-level health
monitoring without hardware watchdog coupling.

## Why self-written?

Searched GitHub (2026-05-02). Found MaJerle/lwwdg (MIT). Similar concept; our tick-based OS-agnostic approach. Self-written for passive-library architecture consistency (no malloc, no globals, callback-injected I/O).

## Dependencies

None.

## API

```c
task_wd_err_t task_watchdog_init(task_watchdog_t *wd);
task_wd_err_t task_watchdog_register(task_watchdog_t *wd, const char *name,
                                     uint32_t timeout_ticks);
task_wd_err_t task_watchdog_feed(task_watchdog_t *wd, const char *name);
void          task_watchdog_check(task_watchdog_t *wd);
void          task_watchdog_tick(task_watchdog_t *wd);
void          task_watchdog_set_callback(task_watchdog_t *wd,
                                         task_wd_callback_t cb, void *ctx);
task_wd_err_t task_watchdog_enable(task_watchdog_t *wd, const char *name);
task_wd_err_t task_watchdog_disable(task_watchdog_t *wd, const char *name);
task_wd_status_t task_watchdog_status(const task_watchdog_t *wd,
                                      const char *name);
uint8_t       task_watchdog_count(const task_watchdog_t *wd);
uint32_t      task_watchdog_expired_count(const task_watchdog_t *wd);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `TASK_WD_MAX_TASKS` | 8 | Maximum number of monitored tasks, at most 255 |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/task_watchdog/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_task_watchdog
```

## Test status

- Unit tests (sim): 19 pass / 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Tick-driven passive design; string-based task lookup;
  callback fires once per expiry (re-feed to reset).

## Responsible human

@user
