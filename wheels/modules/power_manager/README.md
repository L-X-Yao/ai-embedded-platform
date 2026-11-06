# power_manager -- cooperative sleep/wake orchestrator (no OS)

**Status: candidate**
**Layer: L3 (modules)**

Cooperative power management module. Modules register as "voters" that
can veto sleep or request wake. `poll()` evaluates all voters and
recommends the deepest allowed sleep level. The caller (main loop or
idle hook) decides whether to actually enter sleep -- this wheel never
calls HAL sleep directly. Activity timeout prevents crashed modules
from blocking sleep forever.

## Origin

Proactive build for battery-powered IoT devices that need coordinated
power management across multiple subsystems (radio, sensor, UART).

## Why self-written?

existing open-source has a kernel idle mechanism but no voter-based power orchestrator
that works without the VSF scheduler. Self-written for bare-metal and
RTOS-agnostic power coordination.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Tick source injected via `power_manager_io_t` callback.

## API

```c
power_manager_err_t power_manager_init(power_manager_t *pm,
                                       const power_manager_io_t *io);
power_manager_err_t power_manager_register(power_manager_t *pm,
                                           const char *name,
                                           power_level_t max_sleep,
                                           uint32_t timeout_ms,
                                           uint8_t *id_out);
power_manager_err_t power_manager_activity(power_manager_t *pm, uint8_t voter_id);
power_manager_err_t power_manager_release(power_manager_t *pm, uint8_t voter_id);
power_level_t       power_manager_poll(power_manager_t *pm);
power_level_t       power_manager_get_level(const power_manager_t *pm);
uint8_t             power_manager_active_count(const power_manager_t *pm);
bool                power_manager_is_active(const power_manager_t *pm,
                                            uint8_t voter_id);
const char         *power_manager_voter_name(const power_manager_t *pm,
                                             uint8_t voter_id);
```

Sleep levels: `ACTIVE > IDLE > LIGHT_SLEEP > DEEP_SLEEP > SHUTDOWN`

## Usage

```c
power_manager_t pm;
power_manager_init(&pm, &tick_io);

uint8_t uart_id, radio_id;
power_manager_register(&pm, "uart", POWER_LEVEL_IDLE, 5000, &uart_id);
power_manager_register(&pm, "radio", POWER_LEVEL_LIGHT_SLEEP, 0, &radio_id);

/* UART busy -- blocks sleep below IDLE */
power_manager_activity(&pm, uart_id);

/* main loop */
power_level_t level = power_manager_poll(&pm);
if (level >= POWER_LEVEL_DEEP_SLEEP) {
    hal_enter_deep_sleep();
}
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `POWER_MANAGER_MAX_VOTERS` | 16 | Maximum registered voters, at most 255 |

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected tick source)

## Known issues

None.

## How to test

```bash
cd wheels/modules/power_manager/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_power_manager
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
