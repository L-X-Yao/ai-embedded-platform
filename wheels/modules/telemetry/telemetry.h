// SPDX-License-Identifier: MIT
/*
 * telemetry -- periodic telemetry data formatter and publisher
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none -- self-contained)
 * Instances: multi
 *
 * Passive design: application registers named fields, updates values,
 * and calls telemetry_publish() when ready.  Publish formats all fields
 * in the chosen format (JSON / CSV / key-value) and calls the send
 * callback.  No OS, no tasks, no malloc.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef TELEMETRY_MAX_FIELDS
#define TELEMETRY_MAX_FIELDS    16
#endif
#if TELEMETRY_MAX_FIELDS > 255
#error "TELEMETRY_MAX_FIELDS exceeds 255, the most a uint8_t count can hold"
#endif

#ifndef TELEMETRY_BUF_SIZE
#define TELEMETRY_BUF_SIZE      512
#endif

/* ---------- Error codes ---------- */

typedef enum {
    TELEMETRY_OK            =  0,
    TELEMETRY_ERR_PARAM    = -1,
    TELEMETRY_ERR_FULL      = -2,
    TELEMETRY_ERR_SEND      = -3,
    TELEMETRY_ERR_OVERFLOW  = -4,
} telemetry_err_t;

/* ---------- Output format ---------- */

typedef enum {
    TELEMETRY_FMT_JSON,
    TELEMETRY_FMT_CSV,
    TELEMETRY_FMT_KEY_VALUE,
} telemetry_format_t;

/* ---------- Send callback ---------- */

typedef telemetry_err_t (*telemetry_send_fn)(void *ctx, const char *data,
                                            uint16_t len);

/* ---------- Internal types ---------- */

typedef struct {
    const char *name;
    float       value;
    bool        updated;
} telemetry_field_t;

typedef struct {
    telemetry_field_t   fields[TELEMETRY_MAX_FIELDS];
    uint8_t             field_count;
    telemetry_send_fn   send;
    void               *send_ctx;
    telemetry_format_t  format;
    char                buf[TELEMETRY_BUF_SIZE];
    uint32_t            send_count;
    uint32_t            error_count;
} telemetry_t;

/* ---------- API ---------- */

/**
 * Initialize telemetry context with send callback and output format.
 * @thread_safe: no
 */
telemetry_err_t telemetry_init(telemetry_t *t, telemetry_send_fn send,
                               void *ctx, telemetry_format_t fmt);

/**
 * Register a named field. Returns ERR_FULL if table is full.
 *
 * The name is kept by pointer, not copied, and is read again on every publish
 * and every update by name. It must stay valid and unchanged for as long as t
 * is in use: a string literal or a static array. A stack buffer, or one buffer
 * reused for several names, gives wrong or dead names and no error.
 * @thread_safe: no
 */
telemetry_err_t telemetry_register(telemetry_t *t, const char *name);

/**
 * Update a field's value by name. Returns ERR_PARAM if not found.
 * @thread_safe: no
 */
telemetry_err_t telemetry_update(telemetry_t *t, const char *name,
                                 float value);

/**
 * Update a field's value by index (faster, no name lookup).
 * @thread_safe: no
 */
telemetry_err_t telemetry_update_by_index(telemetry_t *t, uint8_t index,
                                          float value);

/**
 * Format all fields and call send callback.
 * @thread_safe: no
 */
telemetry_err_t telemetry_publish(telemetry_t *t, uint32_t timestamp);

/**
 * Get total successful send count.
 * @thread_safe: no
 */
uint32_t telemetry_get_send_count(const telemetry_t *t);

/**
 * Get total error count.
 * @thread_safe: no
 */
uint32_t telemetry_get_error_count(const telemetry_t *t);

/**
 * Reset all field values and counters.
 * @thread_safe: no
 */
void telemetry_reset(telemetry_t *t);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
