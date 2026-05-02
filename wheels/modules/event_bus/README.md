# event_bus

**Status: candidate**
**Layer: L3 (modules)**

Publish/subscribe event dispatcher for decoupled module communication.
Passive -- application calls `event_bus_publish()`, all matching subscribers
are called synchronously in registration order. No heap, no OS.

## Origin

Proactive build for decoupled module communication (2026-05-01).

## Why self-written?

Searched GitHub (2026-05-02). Found MaJerle/lwevt (MIT). Similar concept; our impl compact and follows conventions. Self-written for passive-library architecture consistency (no malloc, no globals, callback-injected I/O).

## Dependencies

None.

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

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `EVENT_BUS_MAX_EVENTS` | 16 | Max distinct event types |
| `EVENT_BUS_MAX_SUBS_PER_EVENT` | 8 | Max subscribers per event |

## Supported platforms

- [ ] Linux host (sim)
- [ ] Windows (TDM-GCC)

## Known issues

None.

## How to test

```bash
cd wheels/modules/event_bus/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **15 C cases** -- init, init null, subscribe+publish, multiple
  subscribers, unsubscribe, no subscribers (silent), max event types, max
  subscribers per event, data passthrough, NULL params, publish to unregistered
  event, subscriber context, unsubscribe preserves others, multiple events
  isolation, null data with zero len

## API convention deviations

None.

## API review

- Proposal date: 2026-05-01
- Reviewer: @user (pending)

## Responsible human

@user
