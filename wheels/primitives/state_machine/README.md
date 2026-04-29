# state_machine

**Status: candidate**
**Layer: L0 (primitives)**

Table-driven finite state machine with optional entry/exit actions per state
and guard conditions on transitions. Pure C99, no heap, fully reentrant.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`state_machine`, `fsm`, `hsm` — found `fsm_rt_t` return type enum in `__type.h`
(on_going/complete/error), but no generic state machine framework or table-driven
FSM engine.

## Origin

Proactive build. Table-driven FSMs are used in protocol parsers, UI flows,
device lifecycle management, and communication state tracking across
virtually all embedded projects.

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

## Tests

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

- Unit tests (sim): PASS (15 C tests, 42 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Table-driven (not switch/case) for testability and separation of logic from structure; states and events are uint16_t (not enum) for flexibility; guard functions return bool; self-transitions do NOT call on_exit/on_enter (consistent with UML semantics for internal transitions); first matching transition wins (priority by table order).

## Responsible human
@user
