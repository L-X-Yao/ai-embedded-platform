# crsf — TODO: one-line description

**Status: candidate**
**Layer: L2 (protocols)**

TODO: 2-3 sentence description.

## Origin

TODO: "Extracted from `projects/<proj>/src/...`" or "Proactive build for <use case>".

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- TODO: list evaluated alternatives and why they were not suitable.
- No suitable zero-dependency C99 implementation found.

Reference: TODO (or "None").

## Dependencies

TODO: list wheel dependencies (or "None.").

## API

```c
/* TODO: document public API */
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| TODO   | TODO    | TODO    |

## Supported platforms

- [x] Host (sim) — TODO: date
- [ ] TODO: real hardware target — not yet verified

## Known issues

TODO.

## How to test

```bash
cd wheels/protocols/crsf/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_crsf
```

## Test status

- Unit tests (sim): TODO
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: TODO
- **Key decisions**: TODO

## Responsible human

@user
