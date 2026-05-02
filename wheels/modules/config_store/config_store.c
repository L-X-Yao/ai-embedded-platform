/*
 * config_store -- configuration management with defaults, validation,
 *                 and storage callbacks
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "config_store.h"
#include <string.h>

/* ---------- Internal helpers ---------- */

static config_entry_t *cs_find(config_store_t *cs, const char *key)
{
    for (uint8_t i = 0; i < cs->count; i++) {
        if (strcmp(cs->entries[i].key, key) == 0) {
            return &cs->entries[i];
        }
    }
    return NULL;
}

static const config_entry_t *cs_find_const(const config_store_t *cs,
                                           const char *key)
{
    for (uint8_t i = 0; i < cs->count; i++) {
        if (strcmp(cs->entries[i].key, key) == 0) {
            return &cs->entries[i];
        }
    }
    return NULL;
}

/* ---------- Public API ---------- */

config_store_err_t config_store_init(config_store_t *cs,
                                     const config_store_storage_t *storage)
{
    if (!cs) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    memset(cs, 0, sizeof(*cs));
    cs->storage = storage;
    return CONFIG_STORE_OK;
}

/* --- Define entries --- */

config_store_err_t config_store_define_int(config_store_t *cs,
                                           const char *key,
                                           int32_t default_val,
                                           int32_t min, int32_t max)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (cs->count >= CONFIG_STORE_MAX_ENTRIES) {
        return CONFIG_STORE_ERR_FULL;
    }

    config_entry_t *e = &cs->entries[cs->count];
    e->key              = key;
    e->type             = CONFIG_TYPE_INT32;
    e->default_val.i32  = default_val;
    e->min_val.i32      = min;
    e->max_val.i32      = max;
    e->current.i32      = default_val;
    e->modified         = false;
    cs->count++;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_define_uint(config_store_t *cs,
                                            const char *key,
                                            uint32_t default_val,
                                            uint32_t min, uint32_t max)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (cs->count >= CONFIG_STORE_MAX_ENTRIES) {
        return CONFIG_STORE_ERR_FULL;
    }

    config_entry_t *e = &cs->entries[cs->count];
    e->key              = key;
    e->type             = CONFIG_TYPE_UINT32;
    e->default_val.u32  = default_val;
    e->min_val.u32      = min;
    e->max_val.u32      = max;
    e->current.u32      = default_val;
    e->modified         = false;
    cs->count++;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_define_float(config_store_t *cs,
                                             const char *key,
                                             float default_val,
                                             float min, float max)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (cs->count >= CONFIG_STORE_MAX_ENTRIES) {
        return CONFIG_STORE_ERR_FULL;
    }

    config_entry_t *e = &cs->entries[cs->count];
    e->key              = key;
    e->type             = CONFIG_TYPE_FLOAT;
    e->default_val.f32  = default_val;
    e->min_val.f32      = min;
    e->max_val.f32      = max;
    e->current.f32      = default_val;
    e->modified         = false;
    cs->count++;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_define_bool(config_store_t *cs,
                                            const char *key,
                                            bool default_val)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (cs->count >= CONFIG_STORE_MAX_ENTRIES) {
        return CONFIG_STORE_ERR_FULL;
    }

    config_entry_t *e = &cs->entries[cs->count];
    e->key            = key;
    e->type           = CONFIG_TYPE_BOOL;
    e->default_val.b  = default_val;
    e->current.b      = default_val;
    e->modified       = false;
    cs->count++;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_define_string(config_store_t *cs,
                                              const char *key,
                                              const char *default_val)
{
    if (!cs || !key || !default_val) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (cs->count >= CONFIG_STORE_MAX_ENTRIES) {
        return CONFIG_STORE_ERR_FULL;
    }
    if (strlen(default_val) >= CONFIG_STORE_STRING_SIZE) {
        return CONFIG_STORE_ERR_RANGE;
    }

    config_entry_t *e = &cs->entries[cs->count];
    e->key  = key;
    e->type = CONFIG_TYPE_STRING;
    strncpy(e->default_val.str, default_val, CONFIG_STORE_STRING_SIZE - 1);
    e->default_val.str[CONFIG_STORE_STRING_SIZE - 1] = '\0';
    strncpy(e->current.str, default_val, CONFIG_STORE_STRING_SIZE - 1);
    e->current.str[CONFIG_STORE_STRING_SIZE - 1] = '\0';
    e->modified = false;
    cs->count++;
    return CONFIG_STORE_OK;
}

/* --- Set values --- */

config_store_err_t config_store_set_int(config_store_t *cs,
                                        const char *key, int32_t val)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    config_entry_t *e = cs_find(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_INT32) {
        return CONFIG_STORE_ERR_TYPE;
    }
    if (val < e->min_val.i32 || val > e->max_val.i32) {
        return CONFIG_STORE_ERR_RANGE;
    }

    e->current.i32 = val;
    e->modified    = (val != e->default_val.i32);
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_set_uint(config_store_t *cs,
                                         const char *key, uint32_t val)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    config_entry_t *e = cs_find(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_UINT32) {
        return CONFIG_STORE_ERR_TYPE;
    }
    if (val < e->min_val.u32 || val > e->max_val.u32) {
        return CONFIG_STORE_ERR_RANGE;
    }

    e->current.u32 = val;
    e->modified    = (val != e->default_val.u32);
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_set_float(config_store_t *cs,
                                          const char *key, float val)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    config_entry_t *e = cs_find(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_FLOAT) {
        return CONFIG_STORE_ERR_TYPE;
    }
    if (val < e->min_val.f32 || val > e->max_val.f32) {
        return CONFIG_STORE_ERR_RANGE;
    }

    e->current.f32 = val;
    e->modified    = (val != e->default_val.f32);
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_set_bool(config_store_t *cs,
                                         const char *key, bool val)
{
    if (!cs || !key) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    config_entry_t *e = cs_find(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_BOOL) {
        return CONFIG_STORE_ERR_TYPE;
    }

    e->current.b = val;
    e->modified  = (val != e->default_val.b);
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_set_string(config_store_t *cs,
                                           const char *key, const char *val)
{
    if (!cs || !key || !val) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    config_entry_t *e = cs_find(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_STRING) {
        return CONFIG_STORE_ERR_TYPE;
    }
    if (strlen(val) >= CONFIG_STORE_STRING_SIZE) {
        return CONFIG_STORE_ERR_RANGE;
    }

    strncpy(e->current.str, val, CONFIG_STORE_STRING_SIZE - 1);
    e->current.str[CONFIG_STORE_STRING_SIZE - 1] = '\0';
    e->modified = (strcmp(e->current.str, e->default_val.str) != 0);
    return CONFIG_STORE_OK;
}

/* --- Get values --- */

config_store_err_t config_store_get_int(const config_store_t *cs,
                                        const char *key, int32_t *val)
{
    if (!cs || !key || !val) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    const config_entry_t *e = cs_find_const(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_INT32) {
        return CONFIG_STORE_ERR_TYPE;
    }

    *val = e->current.i32;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_get_uint(const config_store_t *cs,
                                         const char *key, uint32_t *val)
{
    if (!cs || !key || !val) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    const config_entry_t *e = cs_find_const(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_UINT32) {
        return CONFIG_STORE_ERR_TYPE;
    }

    *val = e->current.u32;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_get_float(const config_store_t *cs,
                                          const char *key, float *val)
{
    if (!cs || !key || !val) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    const config_entry_t *e = cs_find_const(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_FLOAT) {
        return CONFIG_STORE_ERR_TYPE;
    }

    *val = e->current.f32;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_get_bool(const config_store_t *cs,
                                         const char *key, bool *val)
{
    if (!cs || !key || !val) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    const config_entry_t *e = cs_find_const(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_BOOL) {
        return CONFIG_STORE_ERR_TYPE;
    }

    *val = e->current.b;
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_get_string(const config_store_t *cs,
                                           const char *key, char *val,
                                           uint16_t size)
{
    if (!cs || !key || !val || size == 0) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    const config_entry_t *e = cs_find_const(cs, key);
    if (!e) {
        return CONFIG_STORE_ERR_NOT_FOUND;
    }
    if (e->type != CONFIG_TYPE_STRING) {
        return CONFIG_STORE_ERR_TYPE;
    }

    strncpy(val, e->current.str, size - 1);
    val[size - 1] = '\0';
    return CONFIG_STORE_OK;
}

/* --- Persistence --- */

config_store_err_t config_store_save_all(config_store_t *cs)
{
    if (!cs) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (!cs->storage || !cs->storage->save) {
        return CONFIG_STORE_ERR_STORAGE;
    }

    for (uint8_t i = 0; i < cs->count; i++) {
        config_entry_t *e = &cs->entries[i];
        config_store_err_t err = cs->storage->save(
            cs->storage->ctx, e->key,
            (const uint8_t *)&e->current, sizeof(config_value_t));
        if (err != CONFIG_STORE_OK) {
            return CONFIG_STORE_ERR_STORAGE;
        }
    }
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_load_all(config_store_t *cs)
{
    if (!cs) {
        return CONFIG_STORE_ERR_PARAMS;
    }
    if (!cs->storage || !cs->storage->load) {
        return CONFIG_STORE_ERR_STORAGE;
    }

    for (uint8_t i = 0; i < cs->count; i++) {
        config_entry_t *e = &cs->entries[i];
        config_value_t loaded;
        memset(&loaded, 0, sizeof(loaded));
        config_store_err_t err = cs->storage->load(
            cs->storage->ctx, e->key,
            (uint8_t *)&loaded, sizeof(config_value_t));
        if (err != CONFIG_STORE_OK) {
            return CONFIG_STORE_ERR_STORAGE;
        }
        /* validate loaded value against range; revert to default if OOB */
        bool in_range = true;
        switch (e->type) {
        case CONFIG_TYPE_INT32:
            if (loaded.i32 < e->min_val.i32 || loaded.i32 > e->max_val.i32)
                in_range = false;
            break;
        case CONFIG_TYPE_UINT32:
            if (loaded.u32 < e->min_val.u32 || loaded.u32 > e->max_val.u32)
                in_range = false;
            break;
        case CONFIG_TYPE_FLOAT:
            if (loaded.f32 < e->min_val.f32 || loaded.f32 > e->max_val.f32)
                in_range = false;
            break;
        case CONFIG_TYPE_BOOL:
            break;  /* no range constraint for booleans */
        case CONFIG_TYPE_STRING:
            if (strlen(loaded.str) >= CONFIG_STORE_STRING_SIZE)
                in_range = false;
            break;
        }

        if (in_range) {
            e->current = loaded;
        }
        /* else: keep default value */

        /* update modified flag based on type */
        switch (e->type) {
        case CONFIG_TYPE_INT32:
            e->modified = (e->current.i32 != e->default_val.i32);
            break;
        case CONFIG_TYPE_UINT32:
            e->modified = (e->current.u32 != e->default_val.u32);
            break;
        case CONFIG_TYPE_FLOAT:
            e->modified = (e->current.f32 != e->default_val.f32);
            break;
        case CONFIG_TYPE_BOOL:
            e->modified = (e->current.b != e->default_val.b);
            break;
        case CONFIG_TYPE_STRING:
            e->modified = (strcmp(e->current.str,
                                 e->default_val.str) != 0);
            break;
        }
    }
    return CONFIG_STORE_OK;
}

config_store_err_t config_store_reset_defaults(config_store_t *cs)
{
    if (!cs) {
        return CONFIG_STORE_ERR_PARAMS;
    }

    for (uint8_t i = 0; i < cs->count; i++) {
        cs->entries[i].current  = cs->entries[i].default_val;
        cs->entries[i].modified = false;
    }
    return CONFIG_STORE_OK;
}

uint8_t config_store_count(const config_store_t *cs)
{
    if (!cs) {
        return 0;
    }
    return cs->count;
}
