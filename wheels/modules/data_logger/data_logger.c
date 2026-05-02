// SPDX-License-Identifier: MIT
/*
 * data_logger -- record-based data logger for embedded systems
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "data_logger.h"
#include <string.h>
#include <stdio.h>

/* ---------- Internal helpers ---------- */

/*
 * Append text to the record buffer.
 * Returns DATA_LOGGER_OK or DATA_LOGGER_ERR_OVERFLOW.
 */
static data_logger_err_t buf_append(data_logger_t *dl, const char *s,
                                    uint16_t len)
{
    if (dl->buf_pos + len > DATA_LOGGER_BUF_SIZE) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }
    memcpy(dl->buf + dl->buf_pos, s, len);
    dl->buf_pos = (uint16_t)(dl->buf_pos + len);
    return DATA_LOGGER_OK;
}

/*
 * Append a separator comma before each field (not before first field).
 */
static data_logger_err_t buf_comma(data_logger_t *dl)
{
    return buf_append(dl, ",", 1);
}

/*
 * Check common preconditions for add_* functions:
 * - dl not NULL
 * - record is active (between begin/commit)
 * - field_idx < field_count
 */
static data_logger_err_t check_add(const data_logger_t *dl,
                                   data_logger_field_type_t expected)
{
    if (!dl) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (!dl->active) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (dl->field_idx >= dl->field_count) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (dl->fields[dl->field_idx].type != expected) {
        return DATA_LOGGER_ERR_PARAM;
    }
    return DATA_LOGGER_OK;
}

/* ---------- Public API ---------- */

data_logger_err_t data_logger_init(data_logger_t *dl,
                                   const data_logger_storage_t *storage,
                                   const data_logger_field_t *fields,
                                   uint8_t field_count)
{
    if (!dl || !storage || !storage->write) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (field_count > 0 && !fields) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (field_count > DATA_LOGGER_MAX_FIELDS) {
        return DATA_LOGGER_ERR_PARAM;
    }

    memset(dl, 0, sizeof(*dl));
    dl->storage     = storage;
    dl->fields      = fields;
    dl->field_count = field_count;
    return DATA_LOGGER_OK;
}

data_logger_err_t data_logger_begin(data_logger_t *dl, uint32_t timestamp)
{
    if (!dl) {
        return DATA_LOGGER_ERR_PARAM;
    }

    dl->buf_pos   = 0;
    dl->field_idx = 0;
    dl->active    = 1;

    /* Format timestamp as first column */
    char tmp[12];
    int n = snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)timestamp);
    if (n < 0 || (uint16_t)n >= sizeof(tmp)) {
        dl->active = 0;
        return DATA_LOGGER_ERR_OVERFLOW;
    }
    return buf_append(dl, tmp, (uint16_t)n);
}

data_logger_err_t data_logger_add_int(data_logger_t *dl, int32_t value)
{
    data_logger_err_t err = check_add(dl, DATA_LOGGER_FIELD_INT32);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    err = buf_comma(dl);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    char tmp[12];
    int n = snprintf(tmp, sizeof(tmp), "%ld", (long)value);
    if (n < 0 || (uint16_t)n >= sizeof(tmp)) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }

    err = buf_append(dl, tmp, (uint16_t)n);
    if (err == DATA_LOGGER_OK) {
        dl->field_idx++;
    }
    return err;
}

data_logger_err_t data_logger_add_uint(data_logger_t *dl, uint32_t value)
{
    data_logger_err_t err = check_add(dl, DATA_LOGGER_FIELD_UINT32);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    err = buf_comma(dl);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    char tmp[12];
    int n = snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)value);
    if (n < 0 || (uint16_t)n >= sizeof(tmp)) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }

    err = buf_append(dl, tmp, (uint16_t)n);
    if (err == DATA_LOGGER_OK) {
        dl->field_idx++;
    }
    return err;
}

data_logger_err_t data_logger_add_float(data_logger_t *dl, float value)
{
    data_logger_err_t err = check_add(dl, DATA_LOGGER_FIELD_FLOAT);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    err = buf_comma(dl);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    char tmp[20];
    int n = snprintf(tmp, sizeof(tmp), "%.2f", (double)value);
    if (n < 0 || (uint16_t)n >= sizeof(tmp)) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }

    err = buf_append(dl, tmp, (uint16_t)n);
    if (err == DATA_LOGGER_OK) {
        dl->field_idx++;
    }
    return err;
}

data_logger_err_t data_logger_add_string(data_logger_t *dl, const char *value)
{
    data_logger_err_t err = check_add(dl, DATA_LOGGER_FIELD_STRING);
    if (err != DATA_LOGGER_OK) {
        return err;
    }
    if (!value) {
        return DATA_LOGGER_ERR_PARAM;
    }

    err = buf_comma(dl);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    uint16_t len = (uint16_t)strlen(value);
    err = buf_append(dl, value, len);
    if (err == DATA_LOGGER_OK) {
        dl->field_idx++;
    }
    return err;
}

data_logger_err_t data_logger_commit(data_logger_t *dl)
{
    if (!dl) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (!dl->active) {
        return DATA_LOGGER_ERR_PARAM;
    }

    /* Append newline to complete the record */
    data_logger_err_t err = buf_append(dl, "\n", 1);
    if (err != DATA_LOGGER_OK) {
        dl->active = 0;
        return err;
    }

    /* Write to storage */
    err = dl->storage->write(dl->storage->ctx, dl->buf, dl->buf_pos);
    dl->active = 0;
    if (err != DATA_LOGGER_OK) {
        return DATA_LOGGER_ERR_WRITE;
    }

    dl->record_count++;
    return DATA_LOGGER_OK;
}

data_logger_err_t data_logger_flush(data_logger_t *dl)
{
    if (!dl || !dl->storage) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (!dl->storage->flush) {
        return DATA_LOGGER_OK;  /* no flush callback -- nothing to do */
    }
    return dl->storage->flush(dl->storage->ctx);
}

uint32_t data_logger_get_count(const data_logger_t *dl)
{
    if (!dl) {
        return 0;
    }
    return dl->record_count;
}
