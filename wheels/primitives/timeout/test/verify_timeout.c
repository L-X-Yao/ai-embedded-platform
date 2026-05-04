/*
 * verify_timeout.c — CBMC bounded model checking for timeout
 *
 * Verifies expiration logic, wraparound correctness, and elapsed/remaining
 * consistency. Run with:
 *   cbmc verify_timeout.c ../timeout.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../timeout.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

extern uint8_t  nondet_uint8(void);
extern uint32_t nondet_uint32(void);
extern unsigned  nondet_unsigned(void);

/* Proof 1: not expired before deadline, expired at/after deadline */
void verify_expiration_logic(void) {
    timeout_t t;
    uint32_t start    = nondet_uint32();
    uint32_t duration = nondet_uint32();

    /* Avoid zero duration (immediate expiry) and extreme values */
    __CPROVER_assume(duration >= 1 && duration <= 10000);

    timeout_set(&t, start, duration);

    /* Immediately after set, not expired (elapsed = 0 < duration) */
    assert(timeout_expired(&t, start) == false);

    /* One tick before deadline, not expired */
    uint32_t just_before = start + duration - 1;
    assert(timeout_expired(&t, just_before) == false);

    /* At deadline, expired */
    uint32_t at_deadline = start + duration;
    assert(timeout_expired(&t, at_deadline) == true);

    /* After deadline, still expired */
    uint32_t after = start + duration + 1;
    assert(timeout_expired(&t, after) == true);
}

/* Proof 2: 32-bit tick wraparound handled correctly */
void verify_wraparound(void) {
    timeout_t t;

    /* Start near UINT32_MAX, duration crosses wrap boundary */
    uint32_t start = 0xFFFFFFF0u;
    uint32_t duration = 32;
    timeout_set(&t, start, duration);

    /* Before deadline (wrapping into low range) */
    uint32_t before = start + duration - 1; /* wraps around */
    assert(timeout_expired(&t, before) == false);

    /* At deadline (wrapped) */
    uint32_t at = start + duration;
    assert(timeout_expired(&t, at) == true);

    /* Elapsed and remaining are correct through wrap */
    uint32_t mid = start + 16;
    assert(timeout_elapsed(&t, mid) == 16);
    assert(timeout_remaining(&t, mid) == duration - 16);
}

/* Proof 3: elapsed + remaining == duration (before expiry) */
void verify_elapsed_remaining_sum(void) {
    timeout_t t;
    uint32_t start    = nondet_uint32();
    uint32_t duration = nondet_uint32();
    uint32_t now      = nondet_uint32();

    __CPROVER_assume(duration >= 1 && duration <= 10000);

    /* Constrain now to be before expiry */
    uint32_t elapsed = now - start;
    __CPROVER_assume(elapsed < duration);

    timeout_set(&t, start, duration);

    uint32_t e = timeout_elapsed(&t, now);
    uint32_t r = timeout_remaining(&t, now);

    assert(e == elapsed);
    assert(r == duration - elapsed);
    assert(e + r == duration);
}

/* Proof 4: restart and extend behavior */
void verify_restart_extend(void) {
    timeout_t t;
    uint32_t start    = nondet_uint32();
    uint32_t duration = nondet_uint32();
    __CPROVER_assume(duration >= 10 && duration <= 10000);

    timeout_set(&t, start, duration);

    /* Advance 5 ticks */
    uint32_t now = start + 5;

    /* Restart: resets start to now, keeps duration */
    timeout_restart(&t, now);
    assert(timeout_elapsed(&t, now) == 0);
    assert(timeout_expired(&t, now) == false);

    /* New deadline is now + duration */
    assert(timeout_expired(&t, now + duration) == true);
    assert(timeout_expired(&t, now + duration - 1) == false);

    /* Extend: adds to current deadline */
    timeout_set(&t, start, duration);
    uint32_t extra = nondet_uint32();
    __CPROVER_assume(extra >= 1 && extra <= 1000);
    timeout_extend(&t, extra);

    /* New deadline is start + duration + extra */
    uint32_t new_deadline = start + duration + extra;
    assert(timeout_expired(&t, new_deadline) == true);
}

int main(void) {
    verify_expiration_logic();
    verify_wraparound();
    verify_elapsed_remaining_sum();
    verify_restart_extend();
    return 0;
}
