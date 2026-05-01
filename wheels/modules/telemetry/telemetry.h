/*
 * telemetry -- periodic telemetry data formatter and publisher
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- self-contained)
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

#ifndef TELEMETRY_BUF_SIZE
#define TELEMETRY_BUF_SIZE      512
#endif

/* ---------- Error codes ---------- */

typedef enum {
    TELEMETRY_OK            =  0,
    TELEMETRY_ERR_PARAMS    = -1,
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

/** Initialize telemetry context with send callback and output format. */
telemetry_err_t telemetry_init(telemetry_t *t, telemetry_send_fn send,
                               void *ctx, telemetry_format_t fmt);

/** Register a named field. Returns ERR_FULL if table is full. */
telemetry_err_t telemetry_register(telemetry_t *t, const char *name);

/** Update a field's value by name. Returns ERR_PARAMS if not found. */
telemetry_err_t telemetry_update(telemetry_t *t, const char *name,
                                 float value);

/** Update a field's value by index (faster, no name lookup). */
telemetry_err_t telemetry_update_by_index(telemetry_t *t, uint8_t index,
                                          float value);

/** Format all fields and call send callback. */
telemetry_err_t telemetry_publish(telemetry_t *t, uint32_t timestamp);

/** Get total successful send count. */
uint32_t telemetry_get_send_count(const telemetry_t *t);

/** Get total error count. */
uint32_t telemetry_get_error_count(const telemetry_t *t);

/** Reset all field values and counters. */
void telemetry_reset(telemetry_t *t);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
