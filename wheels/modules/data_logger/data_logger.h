// SPDX-License-Identifier: MIT
/*
 * data_logger -- record-based data logger for embedded systems
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none -- self-contained)
 * Instances: multi
 *
 * Passive design: application calls begin/add/commit to build records,
 * logger formats CSV and calls storage callback. Application decides
 * the backend (flash, SD card, UART, etc.).
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef DATA_LOGGER_BUF_SIZE
#define DATA_LOGGER_BUF_SIZE    256
#endif

#ifndef DATA_LOGGER_MAX_FIELDS
#define DATA_LOGGER_MAX_FIELDS  8
#endif

/* ---------- Error codes ---------- */

typedef enum {
    DATA_LOGGER_OK           =  0,
    DATA_LOGGER_ERR_PARAM   = -1,
    DATA_LOGGER_ERR_OVERFLOW = -2,
    DATA_LOGGER_ERR_WRITE    = -3,
} data_logger_err_t;

/* ---------- Storage abstraction ---------- */

typedef struct {
    data_logger_err_t (*write)(void *ctx, const uint8_t *data, uint16_t len);
    data_logger_err_t (*flush)(void *ctx);
    void *ctx;
} data_logger_storage_t;

/* ---------- Field schema ---------- */

typedef enum {
    DATA_LOGGER_FIELD_INT32,
    DATA_LOGGER_FIELD_UINT32,
    DATA_LOGGER_FIELD_FLOAT,
    DATA_LOGGER_FIELD_STRING,
} data_logger_field_type_t;

typedef struct {
    const char             *name;
    data_logger_field_type_t type;
} data_logger_field_t;

/* ---------- Logger context ---------- */

typedef struct {
    const data_logger_storage_t *storage;
    const data_logger_field_t   *fields;
    uint8_t                      field_count;
    uint8_t                      field_idx;
    uint8_t                      buf[DATA_LOGGER_BUF_SIZE];
    uint16_t                     buf_pos;
    uint32_t                     record_count;
    uint8_t                      active;      /* 1 between begin/commit */
} data_logger_t;

/* ---------- API ---------- */

/**
 * Initialize logger with storage backend and field schema.
 * @thread_safe: no
 */
data_logger_err_t data_logger_init(data_logger_t *dl,
                                   const data_logger_storage_t *storage,
                                   const data_logger_field_t *fields,
                                   uint8_t field_count);

/**
 * Begin a new record. Writes timestamp to buffer.
 * @thread_safe: no
 */
data_logger_err_t data_logger_begin(data_logger_t *dl, uint32_t timestamp);

/**
 * Add a signed 32-bit integer field.
 * @thread_safe: no
 */
data_logger_err_t data_logger_add_int(data_logger_t *dl, int32_t value);

/**
 * Add an unsigned 32-bit integer field.
 * @thread_safe: no
 */
data_logger_err_t data_logger_add_uint(data_logger_t *dl, uint32_t value);

/**
 * Add a floating-point field (2 decimal places).
 * @thread_safe: no
 */
data_logger_err_t data_logger_add_float(data_logger_t *dl, float value);

/**
 * Add a string field. It is written unquoted, so a value holding a comma,
 * a double quote, CR or LF is refused with DATA_LOGGER_ERR_PARAM.
 * @thread_safe: no
 */
data_logger_err_t data_logger_add_string(data_logger_t *dl, const char *value);

/**
 * Commit record: calls storage->write() with complete CSV line.
 * @thread_safe: no
 */
data_logger_err_t data_logger_commit(data_logger_t *dl);

/**
 * Flush storage backend.
 * @thread_safe: no
 */
data_logger_err_t data_logger_flush(data_logger_t *dl);

/**
 * Get total committed record count.
 * @thread_safe: no
 */
uint32_t data_logger_get_count(const data_logger_t *dl);

#ifdef __cplusplus
}
#endif

#endif /* DATA_LOGGER_H */
