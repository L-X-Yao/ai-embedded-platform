// SPDX-License-Identifier: MIT
/*
 * font_5x7 — TODO: one-line description
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef FONT_5X7_H
#define FONT_5X7_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    FONT_5X7_OK        =  0,
    FONT_5X7_ERR_PARAM = -1,
} font_5x7_err_t;

/* ---------- instance ---------- */

typedef struct {
    int _placeholder; /* internal: replace with real state */
} font_5x7_t;

/* ---------- API ---------- */

/** Initialize instance. @thread_safe: no @isr_safe: no */
font_5x7_err_t font_5x7_init(font_5x7_t *ctx);

/* TODO: add more API — see api-conventions.md for rules.
 * Add @thread_safe and @isr_safe annotations to every function declaration.
 * Mark internal struct fields with a comment. */

#ifdef __cplusplus
}
#endif

#endif /* FONT_5X7_H */
