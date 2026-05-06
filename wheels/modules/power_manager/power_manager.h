// SPDX-License-Identifier: MIT
/*
 * power_manager — cooperative sleep/wake orchestrator (no OS)
 *
 * Layer:     L3 (modules)
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Modules register as "voters" that can veto sleep or request wake.
 * poll() evaluates all voters and recommends the deepest allowed
 * sleep level. Caller (main loop / idle hook) decides whether to
 * actually enter sleep — this wheel never calls HAL sleep directly.
 *
 * Typical flow:
 *   1. Init + register voters (UART, sensor, radio, ...)
 *   2. Each voter calls activity() when busy, release() when idle
 *   3. Main loop calls poll() → gets recommended sleep level
 *   4. Main loop enters sleep via HAL if level > ACTIVE
 *
 * Activity timeout: voters that call activity() auto-release after
 * their configured timeout_ms, so a crashed module can't block sleep
 * forever.
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- configurable limits ---------- */

#ifndef POWER_MANAGER_MAX_VOTERS
#define POWER_MANAGER_MAX_VOTERS  16
#endif

/* ---------- error codes ---------- */

typedef enum {
    POWER_MANAGER_OK        =  0,
    POWER_MANAGER_ERR_PARAM = -1,
    POWER_MANAGER_ERR_FULL  = -2,
    POWER_MANAGER_ERR_ID    = -3,
} power_manager_err_t;

/* ---------- sleep levels (ordered: deeper = higher value) ---------- */

typedef enum {
    POWER_LEVEL_ACTIVE = 0,
    POWER_LEVEL_IDLE,
    POWER_LEVEL_LIGHT_SLEEP,
    POWER_LEVEL_DEEP_SLEEP,
    POWER_LEVEL_SHUTDOWN,
    POWER_LEVEL_COUNT,
} power_level_t;

/* ---------- HAL injection ---------- */

typedef struct {
    uint32_t (*get_tick_ms)(void *ctx);
    void *ctx;
} power_manager_io_t;

/* ---------- voter (registered module) ---------- */

typedef struct {
    const char *name;
    power_level_t max_sleep;
    uint32_t timeout_ms;
    uint32_t last_activity_ms;
    bool     active;
} power_voter_t;

/* ---------- instance ---------- */

typedef struct {
    const power_manager_io_t *io;         /* internal */
    power_voter_t voters[POWER_MANAGER_MAX_VOTERS]; /* internal */
    uint8_t       voter_count;            /* internal */
    power_level_t current_level;          /* internal */
    uint32_t      idle_since_ms;          /* internal */
} power_manager_t;

/* ---------- API ---------- */

/** Initialize manager. @thread_safe: no @isr_safe: no */
power_manager_err_t power_manager_init(power_manager_t *pm,
                                       const power_manager_io_t *io);

/**
 * Register a voter (module that can veto sleep).
 * @param name       Human-readable label (for debug)
 * @param max_sleep  Deepest sleep this voter allows when active
 * @param timeout_ms Activity auto-expires after this (0 = no timeout)
 * @param id_out     Receives voter ID (0-based index) for activity/release
 * @thread_safe: no @isr_safe: no
 */
power_manager_err_t power_manager_register(power_manager_t *pm,
                                           const char *name,
                                           power_level_t max_sleep,
                                           uint32_t timeout_ms,
                                           uint8_t *id_out);

/** Mark voter as busy (prevents sleep beyond voter's max_sleep). @thread_safe: no @isr_safe: yes */
power_manager_err_t power_manager_activity(power_manager_t *pm, uint8_t voter_id);

/** Mark voter as idle (no longer blocking sleep). @thread_safe: no @isr_safe: yes */
power_manager_err_t power_manager_release(power_manager_t *pm, uint8_t voter_id);

/**
 * Evaluate all voters and return the recommended sleep level.
 * Also expires timed-out activities. Call periodically from main loop.
 * @thread_safe: no @isr_safe: no
 */
power_level_t power_manager_poll(power_manager_t *pm);

/** Get the current recommended level without re-evaluating. @thread_safe: no @isr_safe: yes */
power_level_t power_manager_get_level(const power_manager_t *pm);

/** Get number of currently active voters. @thread_safe: no @isr_safe: yes */
uint8_t power_manager_active_count(const power_manager_t *pm);

/** Check if a specific voter is currently active. @thread_safe: no @isr_safe: yes */
bool power_manager_is_active(const power_manager_t *pm, uint8_t voter_id);

/** Get voter name by ID (for debug/logging). @thread_safe: no @isr_safe: yes */
const char *power_manager_voter_name(const power_manager_t *pm, uint8_t voter_id);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MANAGER_H */
