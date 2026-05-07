// SPDX-License-Identifier: MIT
/*
 * test_dummy1 — TODO: one-line description
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef TEST_DUMMY1_H
#define TEST_DUMMY1_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TEST_DUMMY1_OK        =  0,
    TEST_DUMMY1_ERR_PARAM = -1,
} test_dummy1_err_t;

/* ---------- instance ---------- */

typedef struct {
    int _placeholder; /* internal: replace with real state */
} test_dummy1_t;

/* ---------- API ---------- */

/** Initialize instance. @thread_safe: no @isr_safe: no */
test_dummy1_err_t test_dummy1_init(test_dummy1_t *ctx);

/* TODO: add more API — see api-conventions.md for rules.
 * Add @thread_safe and @isr_safe annotations to every function declaration.
 * Mark internal struct fields with a comment. */

#ifdef __cplusplus
}
#endif

#endif /* TEST_DUMMY1_H */
