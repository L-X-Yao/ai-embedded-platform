# version_info — semantic version comparison and compatibility

**Status: candidate**
**Layer: L3 (modules)**

Parse, compare, and check compatibility of semantic version strings ("major.minor.patch").
Pure functions, no state, no heap, no OS dependency. Suitable for OTA version gates,
firmware compatibility checks, and protocol negotiation.

## Origin

Proactive build for firmware version management (OTA, boot compatibility checks).

## Dependencies

None.

## API

```c
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} version_t;

version_info_err_t version_parse(const char *str, version_t *v);
int                version_compare(const version_t *a, const version_t *b);
bool               version_compatible(const version_t *required, const version_t *actual);
int                version_to_string(const version_t *v, char *buf, size_t buf_size);
bool               version_is_valid(const version_t *v);
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/version_info/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_version_info
```

## Test status

- Unit tests (sim): 26 tests, all passing
- HIL: N/A

## Why not VSF native?

VSF has no semantic version parsing/comparison utility. `vsf_version.h` only defines
the framework's own version as macros, with no runtime parsing or comparison API.

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Pure functions only; version_compatible uses semver major-match rule

## Responsible human

@user
