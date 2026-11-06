// SPDX-License-Identifier: MIT
/*
 * data_logger -- record-based data logger for embedded systems
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "data_logger.h"
#include "contract.h"
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
 * Append one field with its comma and count it.  A field that does not fit
 * leaves the buffer as it was, so a retry cannot add an empty column.
 */
static data_logger_err_t add_field(data_logger_t *dl, const char *s,
                                   uint16_t len)
{
    uint16_t start = dl->buf_pos;

    data_logger_err_t err = buf_comma(dl);
    if (err == DATA_LOGGER_OK) {
        err = buf_append(dl, s, len);
    }
    if (err != DATA_LOGGER_OK) {
        dl->buf_pos = start;
        return err;
    }

    dl->field_idx++;
    return DATA_LOGGER_OK;
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

/*
 * Format value as "%.2f" does, with integer arithmetic.  snprintf will not do:
 * newlib-nano prints nothing for a float unless _printf_float is linked, and
 * full newlib allocates from the heap.  Assumes IEEE 754 binary32.
 * Returns the length, or -1 if the text does not fit in size bytes.
 */
static int fmt_float(char *out, size_t size, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));

    int negative = (int)(bits >> 31);
    uint32_t exponent = (bits >> 23) & 0xFFu;
    uint64_t mantissa = bits & 0x7FFFFFu;

    if (exponent == 0xFFu) {
        const char *text = mantissa != 0 ? "nan" : (negative ? "-inf" : "inf");
        size_t len = strlen(text);
        if (len >= size) {
            return -1;
        }
        memcpy(out, text, len);
        out[len] = '\0';
        return (int)len;
    }

    /* |value| = mantissa * 2^shift exactly, so hundredths can round exactly */
    int shift = -149;
    if (exponent != 0) {
        mantissa |= 0x800000u;
        shift = (int)exponent - 150;
    }
    if (shift > 32) {
        return -1;  /* 2^56 and up: longer than any buffer here, and past 64 bits */
    }

    uint64_t hundredths = mantissa * 100u;
    if (shift >= 0) {
        hundredths <<= shift;
    } else if (shift > -64) {
        uint32_t drop = (uint32_t)-shift;
        uint64_t rest = hundredths & ((UINT64_C(1) << drop) - 1u);
        uint64_t half = UINT64_C(1) << (drop - 1u);
        hundredths >>= drop;
        if (rest > half || (rest == half && (hundredths & 1u) != 0)) {
            hundredths++;  /* ties go to the even digit */
        }
    } else {
        hundredths = 0;
    }

    char digits[20];  /* least significant first; 2^63 has 19 */
    int count = 0;
    do {
        digits[count++] = (char)('0' + (int)(hundredths % 10u));
        hundredths /= 10u;
    } while (hundredths != 0 || count < 3);

    if ((size_t)(negative + count + 1) >= size) {
        return -1;
    }

    int n = 0;
    if (negative) {
        out[n++] = '-';
    }
    while (count > 2) {
        out[n++] = digits[--count];
    }
    out[n++] = '.';
    out[n++] = digits[1];
    out[n++] = digits[0];
    out[n] = '\0';
    return n;
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
    ENSURE(dl->record_count == 0 && dl->buf_pos == 0);
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

    char tmp[12];
    int n = snprintf(tmp, sizeof(tmp), "%ld", (long)value);
    if (n < 0 || (uint16_t)n >= sizeof(tmp)) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }

    return add_field(dl, tmp, (uint16_t)n);
}

data_logger_err_t data_logger_add_uint(data_logger_t *dl, uint32_t value)
{
    data_logger_err_t err = check_add(dl, DATA_LOGGER_FIELD_UINT32);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    char tmp[12];
    int n = snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)value);
    if (n < 0 || (uint16_t)n >= sizeof(tmp)) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }

    return add_field(dl, tmp, (uint16_t)n);
}

data_logger_err_t data_logger_add_float(data_logger_t *dl, float value)
{
    data_logger_err_t err = check_add(dl, DATA_LOGGER_FIELD_FLOAT);
    if (err != DATA_LOGGER_OK) {
        return err;
    }

    char tmp[20];
    int n = fmt_float(tmp, sizeof(tmp), value);
    if (n < 0) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }

    return add_field(dl, tmp, (uint16_t)n);
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
    /* A field is one column and a record one line: these would break out */
    if (value[strcspn(value, ",\"\r\n")] != '\0') {
        return DATA_LOGGER_ERR_PARAM;
    }

    size_t slen = strlen(value);
    if (slen > DATA_LOGGER_BUF_SIZE) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }
    return add_field(dl, value, (uint16_t)slen);
}

data_logger_err_t data_logger_commit(data_logger_t *dl)
{
    if (!dl) {
        return DATA_LOGGER_ERR_PARAM;
    }
    if (!dl->active) {
        return DATA_LOGGER_ERR_PARAM;
    }
    /* A short record would be read as the wrong columns.  It stays open. */
    if (dl->field_idx != dl->field_count) {
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
