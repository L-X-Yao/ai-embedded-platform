# uuid — UUID v4 generation and parsing for embedded systems

**Status: candidate**
**Layer: L0 (primitives)**

RFC 4122 UUID v4 (random-based) implementation. Generates, formats, parses, and compares 128-bit UUIDs. RNG callback injection lets the caller provide any random source (hardware RNG, PRNG, etc.). No malloc, no global state.
## Origin

Proactive build for device identification, session tokens, and message correlation in embedded protocols.


## Why self-written?

Searched GitHub (2026-05-02). Found rxi/uuid4 (MIT). Nearly identical scope for UUID v4 generation. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None.

## API

```c
typedef enum {
    UUID_OK         =  0,
    UUID_ERR_PARAMS = -1,
    UUID_ERR_FORMAT = -2,
} uuid_err_t;

typedef struct {
    uint8_t bytes[16];
} uuid_t;

extern const uuid_t UUID_NIL;

typedef void (*uuid_rng_t)(void *ctx, uint8_t *buf, uint16_t len);

uuid_err_t uuid_v4_generate(uuid_t *uuid, uuid_rng_t rng, void *ctx);
uuid_err_t uuid_to_string(const uuid_t *uuid, char *buf, uint16_t buf_size);
uuid_err_t uuid_from_string(uuid_t *uuid, const char *str);
bool       uuid_equal(const uuid_t *a, const uuid_t *b);
bool       uuid_is_nil(const uuid_t *uuid);
```

## Configuration (compile-time)

None. All behavior is controlled via function parameters.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/uuid/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_uuid
```

## Test status

- Unit tests (sim): 20 passed, 0 failed
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: RNG callback injection (no built-in PRNG), uuid_t as plain 16-byte struct, string format follows RFC 4122 8-4-4-4-12 layout.

## Responsible human

@user
