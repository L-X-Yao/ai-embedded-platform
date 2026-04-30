# log_rate — TODO: one-line description

**Status: candidate**
**Layer: L0 (primitives)**

TODO: 2-3 sentence description.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`TODO: keywords` — TODO: results.

## Origin

TODO: "Extracted from `projects/<proj>/src/...`" or "Proactive build for <use case>".

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
cd wheels/primitives/log_rate/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_log_rate
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
