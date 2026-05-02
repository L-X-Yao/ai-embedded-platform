/*
 * param_table — runtime parameter table with CLI-friendly get/set/list.
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "param_table.h"
#include <string.h>

/* ---------------------------------------------------------------------- */

param_err_t param_table_init(param_table_t *pt)
{
    if (!pt) {
        return PARAM_ERR_PARAMS;
    }
    memset(pt, 0, sizeof(*pt));
    return PARAM_OK;
}

param_err_t param_table_define(param_table_t *pt, const char *key,
                               const char *default_val, bool readonly)
{
    if (!pt || !key) {
        return PARAM_ERR_PARAMS;
    }
    /* Reject if key already exists. */
    if (param_table_find(pt, key) >= 0) {
        return PARAM_ERR_PARAMS;
    }
    if (pt->count >= PARAM_MAX_ENTRIES) {
        return PARAM_ERR_FULL;
    }
    if (default_val && strlen(default_val) >= PARAM_VALUE_SIZE) {
        return PARAM_ERR_OVERFLOW;
    }

    param_entry_t *e = &pt->entries[pt->count];
    e->key       = key;
    e->readonly  = readonly;
    e->on_change = NULL;
    e->change_ctx = NULL;

    if (default_val) {
        strncpy(e->value, default_val, PARAM_VALUE_SIZE - 1);
        e->value[PARAM_VALUE_SIZE - 1] = '\0';
    } else {
        e->value[0] = '\0';
    }

    pt->count++;
    return PARAM_OK;
}

param_err_t param_table_set(param_table_t *pt, const char *key,
                            const char *value)
{
    if (!pt || !key || !value) {
        return PARAM_ERR_PARAMS;
    }

    int idx = param_table_find(pt, key);
    if (idx < 0) {
        return PARAM_ERR_NOT_FOUND;
    }

    param_entry_t *e = &pt->entries[idx];
    if (e->readonly) {
        return PARAM_ERR_READONLY;
    }
    if (strlen(value) >= PARAM_VALUE_SIZE) {
        return PARAM_ERR_OVERFLOW;
    }

    char old_val[PARAM_VALUE_SIZE];
    if (e->on_change) {
        memcpy(old_val, e->value, PARAM_VALUE_SIZE);
    }

    strncpy(e->value, value, PARAM_VALUE_SIZE - 1);
    e->value[PARAM_VALUE_SIZE - 1] = '\0';

    if (e->on_change) {
        e->on_change(e->change_ctx, key, old_val, e->value);
    }

    return PARAM_OK;
}

param_err_t param_table_get(const param_table_t *pt, const char *key,
                            char *value, uint16_t size)
{
    if (!pt || !key || !value || size == 0) {
        return PARAM_ERR_PARAMS;
    }

    int idx = param_table_find(pt, key);
    if (idx < 0) {
        return PARAM_ERR_NOT_FOUND;
    }

    size_t len = strlen(pt->entries[idx].value);
    if (len >= size) {
        return PARAM_ERR_OVERFLOW;
    }

    memcpy(value, pt->entries[idx].value, len + 1);
    return PARAM_OK;
}

param_err_t param_table_set_callback(param_table_t *pt, const char *key,
                                     param_change_fn fn, void *ctx)
{
    if (!pt || !key) {
        return PARAM_ERR_PARAMS;
    }

    int idx = param_table_find(pt, key);
    if (idx < 0) {
        return PARAM_ERR_NOT_FOUND;
    }

    pt->entries[idx].on_change  = fn;
    pt->entries[idx].change_ctx = ctx;
    return PARAM_OK;
}

int param_table_find(const param_table_t *pt, const char *key)
{
    if (!pt || !key) {
        return -1;
    }
    for (uint8_t i = 0; i < pt->count; i++) {
        if (pt->entries[i].key && strcmp(pt->entries[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

uint8_t param_table_count(const param_table_t *pt)
{
    if (!pt) {
        return 0;
    }
    return pt->count;
}

int param_table_list(const param_table_t *pt, char *buf, uint16_t size)
{
    if (!pt || !buf || size == 0) {
        return -1;
    }

    int written = 0;
    for (uint8_t i = 0; i < pt->count; i++) {
        const param_entry_t *e = &pt->entries[i];
        /* Calculate required space: key + '=' + value + '\n' + NUL */
        size_t key_len = strlen(e->key);
        size_t val_len = strlen(e->value);
        size_t line_len = key_len + 1 + val_len + 1; /* key=value\n */

        if ((size_t)written + line_len >= size) {
            /* Not enough room — NUL-terminate what we have and stop. */
            buf[written] = '\0';
            return -1;
        }

        memcpy(&buf[written], e->key, key_len);
        written += (int)key_len;
        buf[written++] = '=';
        memcpy(&buf[written], e->value, val_len);
        written += (int)val_len;
        buf[written++] = '\n';
    }

    buf[written] = '\0';
    return written;
}
