# rate_limiter — generic token bucket rate limiter

**Status: candidate**
**Layer: L0 (primitives)**

Classic token bucket algorithm. Tokens refill at a fixed rate per tick; `try()` consumes a
token if available. Passive — caller supplies `current_tick` from any time source. Fixed-point
(x256) internal precision avoids floating point while supporting sub-tick accumulation.

## Origin

Proactive build for API throttling, retry backoff, resource guarding. Generic counterpart to
`log_rate` (which is log-sink-specific).

## Why self-written?

Searched GitHub (2026-05-02). No lightweight zero-dependency C99 implementation found. All search results are server-side Go/Java rate limiters; no embedded C equivalent. Self-written to match platform conventions.

## Dependencies

None.

## API

```c
typedef enum {
    RATE_LIMITER_OK         =  0,
    RATE_LIMITER_ERR_PARAM  = -1,
    RATE_LIMITER_DENIED     = -2,
} rate_limiter_err_t;

typedef struct {
    uint32_t tokens;          /* current token count (fixed-point, x256) */
    uint32_t max_tokens;      /* bucket capacity (fixed-point, x256) */
    uint32_t refill_rate;     /* tokens added per tick (fixed-point, x256) */
    uint32_t last_tick;       /* last refill timestamp */
    uint32_t allowed_count;   /* lifetime allowed calls */
    uint32_t denied_count;    /* lifetime denied calls */
} rate_limiter_t;

/* Init: max_tokens = bucket capacity, tokens_per_tick = refill rate.
 * Both must be > 0. Bucket starts full. */
rate_limiter_err_t rate_limiter_init(rate_limiter_t *rl, uint32_t max_tokens,
                                     uint32_t tokens_per_tick);

/* Consume 1 token. Returns OK or DENIED. */
rate_limiter_err_t rate_limiter_try(rate_limiter_t *rl, uint32_t current_tick);

/* Consume N tokens atomically. No partial consume. */
rate_limiter_err_t rate_limiter_try_n(rate_limiter_t *rl, uint32_t current_tick,
                                      uint32_t n);

/* Check without consuming. */
bool rate_limiter_can_proceed(const rate_limiter_t *rl, uint32_t current_tick);

/* Reset bucket to full, clear stats. */
void rate_limiter_reset(rate_limiter_t *rl);

/* Accessors. */
uint32_t rate_limiter_allowed(const rate_limiter_t *rl);
uint32_t rate_limiter_denied(const rate_limiter_t *rl);
uint32_t rate_limiter_available(const rate_limiter_t *rl);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `RATE_LIMITER_SCALE` | 256 | Fixed-point multiplier for sub-tick precision |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified (pure C99, expected portable)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/rate_limiter/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_rate_limiter
```

## Test status

- Unit tests (sim): 16 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed-point x256 for sub-tick precision; passive tick injection (no OS dependency); no partial consume on try_n; zero-init struct safe to query but not to use.

## Responsible human

@L-X-Yao
