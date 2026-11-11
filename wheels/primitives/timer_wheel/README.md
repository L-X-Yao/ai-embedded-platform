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


## Why self-written?

Searched GitHub (2026-05-02). Referenced wahern/timeout (MIT) for algorithm verification. wahern/timeout is a hierarchical timing wheel; our simple single-level version is a deliberate simplification for typical embedded timer counts. Self-written for platform convention consistency; algorithm cross-validated against reference.

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

- `add` and `cancel` must run in the tick's context, or with its interrupt
  masked. `add` reads the cursor, so a tick landing inside an unmasked `add`
  makes the timer fire a whole revolution late.

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

- Unit tests (sim): PASS (18 C tests)
- Race test (sim): `test_race_timer_wheel`, a host thread for the tick, built -O3 -flto
- HIL: N/A

## Migration

### Timers added from a callback wait their full delay (2026-11)

- **What changed:** A timer added from inside a callback now always fires its full delay after the current tick, whatever its id. If it landed in the slot being scanned with a higher id, it used to fire on the same tick or a revolution early. `timer_wheel_tick` now marks what is due, then fires it. The new `due` flag sits in padding: `timer_wheel_entry_t` stays 20 bytes on a 32-bit target.
- **What a caller has to do:** Nothing, unless a delay was shortened to allow for the old early firing.
- **If they do nothing:** Such a timer fires later than it did, at the tick asked for.

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Caller provides slot and timer storage arrays (no heap); timer IDs are caller-chosen (0..max_timers-1) for direct indexing; timers are one-shot only (repeating can be implemented by re-adding in the callback); tick is O(max_timers) — scans all entries each tick (linear, fast for typical embedded sizes up to a few hundred); add/cancel are O(1); add/cancel from within expiry callbacks is allowed.

## Migration

**2026-11: a timer is published only once it is whole.** Nothing ordered the
fields around `active`, so a tick beside `add` could call a NULL callback, and a
retry of `add` could hang.

- What changed: `active` is `volatile uint8_t`, not `bool`, because GCC 13's
  `-fsanitize=bool` drops the volatile from a bool load. `current_pos` and
  `count` are volatile. The structs keep their size. No function changed.
- What a caller has to do: if tick runs in an ISR and `add` or `cancel` run
  elsewhere, mask that interrupt around each call. On a compiler other than GCC
  or Clang, define `TIMER_WHEEL_BARRIER()`.
- If they do nothing: code running everything in one context is unaffected.
  Unmasked `add` no longer crashes, but a timer can still fire a revolution late.

## Responsible human

@L-X-Yao
