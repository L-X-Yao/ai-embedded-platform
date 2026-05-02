/*
 * error_handler -- fault logging and recovery module for embedded systems
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- self-contained)
 *
 * Passive design: application calls error_handler_report() when errors
 * occur. Ring buffer stores records, oldest overwritten when full.
 * Fatal errors trigger a callback. Storage callbacks enable persistence
 * across reboots.
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef ERROR_HANDLER_MAX_RECORDS
#define ERROR_HANDLER_MAX_RECORDS 16
#endif

/* ---------- Error codes ---------- */

typedef enum {
    ERROR_HANDLER_OK          =  0,
    ERROR_HANDLER_ERR_PARAMS  = -1,
    ERROR_HANDLER_ERR_FULL    = -2,
    ERROR_HANDLER_ERR_STORAGE = -3,
} error_handler_err_t;

/* ---------- Severity levels ---------- */

typedef enum {
    ERROR_SEVERITY_INFO    = 0,
    ERROR_SEVERITY_WARNING,
    ERROR_SEVERITY_ERROR,
    ERROR_SEVERITY_FATAL,
} error_severity_t;

/* ---------- Error record ---------- */

typedef struct {
    uint32_t         timestamp;
    uint16_t         code;
    error_severity_t severity;
    uint32_t         context;   /* extra info (register value, address, etc.) */
} error_record_t;

/* ---------- Storage abstraction ---------- */

typedef struct {
    error_handler_err_t (*save)(void *ctx, const error_record_t *records,
                                uint8_t count);
    error_handler_err_t (*load)(void *ctx, error_record_t *records,
                                uint8_t max, uint8_t *count);
    void *ctx;
} error_handler_storage_t;

/* ---------- Fatal callback ---------- */

typedef void (*error_handler_callback_t)(void *ctx,
                                         const error_record_t *record);

/* ---------- Handler context ---------- */

typedef struct {
    error_record_t               records[ERROR_HANDLER_MAX_RECORDS];
    uint8_t                      count;
    uint8_t                      head;
    const error_handler_storage_t *storage;
    error_handler_callback_t     on_fatal;
    void                        *fatal_ctx;
    uint32_t                     total_errors;
    uint32_t                     total_fatals;
} error_handler_t;

/* ---------- API ---------- */

/**
 * Initialize handler. storage may be NULL (no persistence).
 * @thread_safe: no
 */
error_handler_err_t error_handler_init(error_handler_t *eh,
                                       const error_handler_storage_t *storage);

/**
 * Report an error. Overwrites oldest record when buffer is full.
 * @thread_safe: no
 */
error_handler_err_t error_handler_report(error_handler_t *eh, uint16_t code,
                                          error_severity_t severity,
                                          uint32_t timestamp, uint32_t context);

/**
 * Set callback invoked on FATAL errors.
 * @thread_safe: no
 */
void error_handler_set_fatal_callback(error_handler_t *eh,
                                       error_handler_callback_t cb, void *ctx);

/**
 * Return number of records currently stored.
 * @thread_safe: no
 */
uint8_t error_handler_get_count(const error_handler_t *eh);

/**
 * Get record by index (0 = oldest).
 * @thread_safe: no
 */
error_handler_err_t error_handler_get(const error_handler_t *eh, uint8_t index,
                                       error_record_t *record);

/**
 * Persist current records via storage callbacks.
 * @thread_safe: no
 */
error_handler_err_t error_handler_save(error_handler_t *eh);

/**
 * Load records from storage, replacing current buffer.
 * @thread_safe: no
 */
error_handler_err_t error_handler_load(error_handler_t *eh);

/**
 * Clear all records and reset counters.
 * @thread_safe: no
 */
void error_handler_clear(error_handler_t *eh);

/**
 * Return lifetime count of all reported errors.
 * @thread_safe: no
 */
uint32_t error_handler_total_errors(const error_handler_t *eh);

/**
 * Return lifetime count of FATAL errors.
 * @thread_safe: no
 */
uint32_t error_handler_total_fatals(const error_handler_t *eh);

#ifdef __cplusplus
}
#endif

#endif /* ERROR_HANDLER_H */
