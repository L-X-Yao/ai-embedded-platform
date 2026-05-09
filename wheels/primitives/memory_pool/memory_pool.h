// SPDX-License-Identifier: MIT
/*
 * memory_pool — TODO: one-line description
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MEMORY_POOL_OK        =  0,
    MEMORY_POOL_ERR_PARAM = -1,
} memory_pool_err_t;

/* ---------- instance ---------- */

typedef struct {
    int _placeholder; /* internal: replace with real state */
} memory_pool_t;

/* ---------- API ---------- */

/** Initialize instance. @thread_safe: no @isr_safe: no */
memory_pool_err_t memory_pool_init(memory_pool_t *ctx);

/* TODO: add more API — see api-conventions.md for rules.
 * Add @thread_safe and @isr_safe annotations to every function declaration.
 * Mark internal struct fields with a comment. */

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_POOL_H */
