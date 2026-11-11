# state_machine

**Status: candidate**
**Layer: L0 (primitives)**

Table-driven finite state machine with optional entry/exit actions per state
and guard conditions on transitions. Pure C99, no heap. Machines are
independent of each other, but one machine runs one transition at a time.
## Origin

Proactive build. Table-driven FSMs are used in protocol parsers, UI flows,
device lifecycle management, and communication state tracking across
virtually all embedded projects.


## Why self-written?

Searched GitHub (2026-05-02). Found VPavlusha/FSM (MIT). Similar no-alloc MIT FSM; roughly equivalent scope. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`.

## API

```c
state_machine_err_t state_machine_init(state_machine_t *ctx, const sm_cfg_t *cfg,
                                       sm_state_t initial_state);
state_machine_err_t state_machine_process(state_machine_t *ctx, sm_event_t event);
sm_state_t          state_machine_current(const state_machine_t *ctx);
uint32_t            state_machine_transition_count(const state_machine_t *ctx);
state_machine_err_t state_machine_reset(state_machine_t *ctx, sm_state_t state);
```

## Configuration (compile-time)

None. States, events, transitions, and actions are all defined at runtime via
`sm_cfg_t`, `sm_transition_t`, and `sm_state_def_t` structs.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/state_machine/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_state_machine
```

42 assertions across 15 test cases: init with entry action, null safety (6 paths),
basic transition, full cycle (RED→GREEN→YELLOW→RED), entry/exit action callbacks,
emergency transition, no-transition error, unknown event, guard pass, guard block,
self-transition (no entry/exit but action fires), reset, no state defs, no
transitions, multiple cycles (9 transitions).

## Test status

- Unit tests (sim): PASS (24 C tests)
- HIL: N/A

## Migration

### Nested process and reset are refused (2026-11)

- **What changed:** `state_machine_process` and `state_machine_reset` return the new `STATE_MACHINE_ERR_BUSY` when called from a guard, action, on_enter or on_exit of the same machine during a transition, and change nothing. The new `in_process` flag sits in padding: `state_machine_t` stays 28 bytes on a 32-bit target. `state_machine_process` is ISR safe only if its callbacks are.
- **What a caller has to do:** Do not drive a machine from its own callbacks. Put the event in a queue, such as the `event_queue` wheel, and process it after the outer call returns. The on_enter run by init and reset is not a transition and may still call process.
- **If they do nothing:** The nested event is refused and not kept, and the machine stays where the outer transition put it. A table that chained states through on_enter worked outside contract builds and now stops one state early. A posting action or on_exit used to corrupt the state or overflow the stack.

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Table-driven (not switch/case) for testability and separation of logic from structure; states and events are uint16_t (not enum) for flexibility; guard functions return bool; self-transitions do NOT call on_exit/on_enter (consistent with UML semantics for internal transitions); first matching transition wins (priority by table order).

## Responsible human
@L-X-Yao
