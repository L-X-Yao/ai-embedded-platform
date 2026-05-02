// SPDX-License-Identifier: MIT
/*
 * telemetry -- periodic telemetry data formatter and publisher
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "telemetry.h"
#include <stdio.h>
#include <string.h>

/* ---------- Internal helpers ---------- */

/*
 * Append string to buffer, respecting capacity.
 * Returns number of chars written (0 if would overflow).
 */
static int buf_append(char *buf, int pos, int cap, const char *s)
{
    int len = (int)strlen(s);
    if (pos + len >= cap) {
        return -1;  /* would overflow */
    }
    memcpy(buf + pos, s, (size_t)len);
    return len;
}

/*
 * Format a float with up to 2 decimal places, trimming trailing zeros.
 * E.g. 25.50 -> "25.5", 60.00 -> "60", 1.23 -> "1.23"
 */
static int fmt_float(char *out, int size, float val)
{
    int n = snprintf(out, (size_t)size, "%.2f", (double)val);
    if (n <= 0 || n >= size) {
        return n;
    }
    /* trim trailing zeros after decimal point */
    char *dot = strchr(out, '.');
    if (dot) {
        char *end = out + n - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
            n--;
        }
        if (end == dot) {
            *end = '\0';
            n--;
        }
    }
    return n;
}

/* ---------- Format functions ---------- */

static int fmt_json(telemetry_t *t, uint32_t timestamp)
{
    int pos = 0;
    int cap = TELEMETRY_BUF_SIZE;
    char tmp[32];
    int n;

    /* opening brace + timestamp */
    n = snprintf(tmp, sizeof(tmp), "{\"ts\":%lu", (unsigned long)timestamp);
    if (n < 0 || (size_t)n >= sizeof(tmp)) { return -1; }
    n = buf_append(t->buf, pos, cap, tmp);
    if (n < 0) { return -1; }
    pos += n;

    for (uint8_t i = 0; i < t->field_count; i++) {
        char fval[32];
        fmt_float(fval, (int)sizeof(fval), t->fields[i].value);
        n = snprintf(tmp, sizeof(tmp), ",\"%s\":%s",
                     t->fields[i].name, fval);
        if (n < 0 || (size_t)n >= sizeof(tmp)) { return -1; }
        n = buf_append(t->buf, pos, cap, tmp);
        if (n < 0) { return -1; }
        pos += n;
    }

    n = buf_append(t->buf, pos, cap, "}");
    if (n < 0) { return -1; }
    pos += n;

    t->buf[pos] = '\0';
    return pos;
}

static int fmt_csv(telemetry_t *t, uint32_t timestamp)
{
    int pos = 0;
    int cap = TELEMETRY_BUF_SIZE;
    char tmp[32];
    int n;

    /* timestamp first */
    n = snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)timestamp);
    if (n < 0 || (size_t)n >= sizeof(tmp)) { return -1; }
    n = buf_append(t->buf, pos, cap, tmp);
    if (n < 0) { return -1; }
    pos += n;

    for (uint8_t i = 0; i < t->field_count; i++) {
        char fval[32];
        fmt_float(fval, (int)sizeof(fval), t->fields[i].value);
        n = snprintf(tmp, sizeof(tmp), ",%s", fval);
        if (n < 0 || (size_t)n >= sizeof(tmp)) { return -1; }
        n = buf_append(t->buf, pos, cap, tmp);
        if (n < 0) { return -1; }
        pos += n;
    }

    t->buf[pos] = '\0';
    return pos;
}

static int fmt_key_value(telemetry_t *t, uint32_t timestamp)
{
    int pos = 0;
    int cap = TELEMETRY_BUF_SIZE;
    char tmp[64];
    int n;

    /* timestamp first */
    n = snprintf(tmp, sizeof(tmp), "ts=%lu", (unsigned long)timestamp);
    if (n < 0 || (size_t)n >= sizeof(tmp)) { return -1; }
    n = buf_append(t->buf, pos, cap, tmp);
    if (n < 0) { return -1; }
    pos += n;

    for (uint8_t i = 0; i < t->field_count; i++) {
        char fval[32];
        fmt_float(fval, (int)sizeof(fval), t->fields[i].value);
        n = snprintf(tmp, sizeof(tmp), " %s=%s",
                     t->fields[i].name, fval);
        if (n < 0 || (size_t)n >= sizeof(tmp)) { return -1; }
        n = buf_append(t->buf, pos, cap, tmp);
        if (n < 0) { return -1; }
        pos += n;
    }

    t->buf[pos] = '\0';
    return pos;
}

/* ---------- Public API ---------- */

telemetry_err_t telemetry_init(telemetry_t *t, telemetry_send_fn send,
                               void *ctx, telemetry_format_t fmt)
{
    if (!t || !send) {
        return TELEMETRY_ERR_PARAM;
    }

    memset(t, 0, sizeof(*t));
    t->send     = send;
    t->send_ctx = ctx;
    t->format   = fmt;
    return TELEMETRY_OK;
}

telemetry_err_t telemetry_register(telemetry_t *t, const char *name)
{
    if (!t || !name) {
        return TELEMETRY_ERR_PARAM;
    }
    if (t->field_count >= TELEMETRY_MAX_FIELDS) {
        return TELEMETRY_ERR_FULL;
    }

    telemetry_field_t *f = &t->fields[t->field_count];
    f->name    = name;
    f->value   = 0.0f;
    f->updated = false;
    t->field_count++;
    return TELEMETRY_OK;
}

telemetry_err_t telemetry_update(telemetry_t *t, const char *name,
                                 float value)
{
    if (!t || !name) {
        return TELEMETRY_ERR_PARAM;
    }

    for (uint8_t i = 0; i < t->field_count; i++) {
        if (strcmp(t->fields[i].name, name) == 0) {
            t->fields[i].value   = value;
            t->fields[i].updated = true;
            return TELEMETRY_OK;
        }
    }
    return TELEMETRY_ERR_PARAM;
}

telemetry_err_t telemetry_update_by_index(telemetry_t *t, uint8_t index,
                                          float value)
{
    if (!t || index >= t->field_count) {
        return TELEMETRY_ERR_PARAM;
    }

    t->fields[index].value   = value;
    t->fields[index].updated = true;
    return TELEMETRY_OK;
}

telemetry_err_t telemetry_publish(telemetry_t *t, uint32_t timestamp)
{
    if (!t) {
        return TELEMETRY_ERR_PARAM;
    }

    int len;

    switch (t->format) {
    case TELEMETRY_FMT_JSON:
        len = fmt_json(t, timestamp);
        break;
    case TELEMETRY_FMT_CSV:
        len = fmt_csv(t, timestamp);
        break;
    case TELEMETRY_FMT_KEY_VALUE:
        len = fmt_key_value(t, timestamp);
        break;
    default:
        return TELEMETRY_ERR_PARAM;
    }

    if (len < 0) {
        t->error_count++;
        return TELEMETRY_ERR_OVERFLOW;
    }

    telemetry_err_t err = t->send(t->send_ctx, t->buf, (uint16_t)len);
    if (err != TELEMETRY_OK) {
        t->error_count++;
        return TELEMETRY_ERR_SEND;
    }

    /* clear updated flags after successful send */
    for (uint8_t i = 0; i < t->field_count; i++) {
        t->fields[i].updated = false;
    }

    t->send_count++;
    return TELEMETRY_OK;
}

uint32_t telemetry_get_send_count(const telemetry_t *t)
{
    return t ? t->send_count : 0;
}

uint32_t telemetry_get_error_count(const telemetry_t *t)
{
    return t ? t->error_count : 0;
}

void telemetry_reset(telemetry_t *t)
{
    if (!t) {
        return;
    }
    for (uint8_t i = 0; i < t->field_count; i++) {
        t->fields[i].value   = 0.0f;
        t->fields[i].updated = false;
    }
    t->send_count  = 0;
    t->error_count = 0;
}
