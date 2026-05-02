# event_bus

**STATUS: candidate**
**Layer: L3 (modules)**

Publish/subscribe event dispatcher for decoupled module communication.
Passive -- application calls `event_bus_publish()`, all matching subscribers
are called synchronously in registration order. No heap, no OS.

## Why not VSF native?

Checked VSF source (2026-05-01): VSF has vsf_eda (event-driven architecture)
which is tightly coupled to the VSF kernel scheduler. This wheel provides a
zero-dependency synchronous pub/sub that works on bare metal or any RTOS.

## API

```c
event_bus_err_t event_bus_init(event_bus_t *bus);
event_bus_err_t event_bus_subscribe(event_bus_t *bus, uint16_t event_id,
                                    event_bus_cb_t cb, void *ctx);
event_bus_err_t event_bus_unsubscribe(event_bus_t *bus, uint16_t event_id,
                                      event_bus_cb_t cb, void *ctx);
event_bus_err_t event_bus_publish(event_bus_t *bus, uint16_t event_id,
                                  const void *data, size_t data_len);
```

## Compile-time config

| Macro | Default | Description |
|-------|---------|-------------|
| `EVENT_BUS_MAX_EVENTS` | 16 | Max distinct event types |
| `EVENT_BUS_MAX_SUBS_PER_EVENT` | 8 | Max subscribers per event |

## Test status

- Unit tests: **15 C cases** -- init, init null, subscribe+publish, multiple
  subscribers, unsubscribe, no subscribers (silent), max event types, max
  subscribers per event, data passthrough, NULL params, publish to unregistered
  event, subscriber context, unsubscribe preserves others, multiple events
  isolation, null data with zero len

## Dependencies

None.

## How to test

```bash
cd wheels/modules/event_bus/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Origin

Proactive build for decoupled module communication (2026-05-01).

## Known issues

None.

## Supported platforms

- [ ] Linux host (sim)
- [ ] Windows (TDM-GCC)

## API review

- Proposal date: 2026-05-01
- Reviewer: @user (pending)

## API convention deviations

None.

## Responsible human

@user
