// SPDX-License-Identifier: MIT
/*
 * double_buffer — TODO: one-line description
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    DOUBLE_BUFFER_OK        =  0,
    DOUBLE_BUFFER_ERR_PARAM = -1,
} double_buffer_err_t;

/* ---------- instance ---------- */

typedef struct {
    int _placeholder; /* internal: replace with real state */
} double_buffer_t;

/* ---------- API ---------- */

/** Initialize instance. @thread_safe: no @isr_safe: no */
double_buffer_err_t double_buffer_init(double_buffer_t *ctx);

/* TODO: add more API — see api-conventions.md for rules.
 * Add @thread_safe and @isr_safe annotations to every function declaration.
 * Mark internal struct fields with a comment. */

#ifdef __cplusplus
}
#endif

#endif /* DOUBLE_BUFFER_H */
