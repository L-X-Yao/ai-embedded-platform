# timer_wheel

**Status: candidate**
**Layer: L0 (primitives)**

Hashed timing wheel for efficient timeout management. Timers are hashed into
wheel slots by `(current_pos + ticks) % num_slots`; each `tick()` advances
the cursor and fires expired entries. add / cancel are O(1); tick is
O(max_timers) due to linear scan over the timer array.
Passive tick-driven design with zero heap allocation.

## Origin

Proactive build. Timing wheels are the standard data structure for managing
large numbers of concurrent timeouts efficiently (network retransmits,
protocol state machines, connection keepalives, sensor polling schedules).

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

## API

```c
timer_wheel_err_t timer_wheel_init(timer_wheel_t *tw,
                                   timer_wheel_slot_t *slots,
                                   uint32_t num_slots,
                                   timer_wheel_entry_t *timers,
                                   uint32_t max_timers);

timer_wheel_err_t timer_wheel_add(timer_wheel_t *tw,
                                  uint32_t id,
                                  uint32_t ticks,
                                  timer_wheel_cb_t callback,
                                  void *ctx);

timer_wheel_err_t timer_wheel_cancel(timer_wheel_t *tw, uint32_t id);

void              timer_wheel_tick(timer_wheel_t *tw);
```

## Configuration (compile-time)

No compile-time macros. Slot count and max timers are set at init via
caller-provided storage arrays, allowing multiple independent wheels with
different sizes.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/timer_wheel/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_timer_wheel
```

16 tests covering: init, NULL safety (14 paths), add+expire, cancel,
multiple timers in same slot, wraparound (delay > num_slots), re-add after
expire, re-add after cancel, tick empty wheel, max timers full, user context
passthrough, delay == num_slots, delay == 1, multiple different delays,
cancel-inactive error, large multi-revolution wraparound.

## Test status

- Unit tests (sim): PASS (16 C tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Caller provides slot and timer storage arrays (no heap); timer IDs are caller-chosen (0..max_timers-1) for direct indexing; timers are one-shot only (repeating can be implemented by re-adding in the callback); tick is O(max_timers) — scans all entries each tick (linear, fast for typical embedded sizes up to a few hundred); add/cancel are O(1); add/cancel from within expiry callbacks is allowed.

## Responsible human

@user
