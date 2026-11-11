// SPDX-License-Identifier: MIT
/*
 * param_table — runtime parameter table with CLI-friendly get/set/list.
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef PARAM_TABLE_H
#define PARAM_TABLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Error codes
 * ====================================================================== */

typedef enum {
    PARAM_OK             =  0,
    PARAM_ERR_PARAM     = -1,   /* NULL pointer or invalid argument      */
    PARAM_ERR_FULL       = -2,   /* table has no free slots                */
    PARAM_ERR_NOT_FOUND  = -3,   /* key does not exist in table            */
    PARAM_ERR_READONLY   = -4,   /* attempted write to readonly parameter  */
    PARAM_ERR_OVERFLOW   = -5,   /* value exceeds PARAM_VALUE_SIZE or buf  */
} param_err_t;

/* ======================================================================
 * Compile-time configuration
 * ====================================================================== */

#ifndef PARAM_MAX_ENTRIES
#define PARAM_MAX_ENTRIES  32
#endif
#if PARAM_MAX_ENTRIES > 255
#error "PARAM_MAX_ENTRIES exceeds 255, the most a uint8_t count can hold"
#endif

#ifndef PARAM_VALUE_SIZE
#define PARAM_VALUE_SIZE   32
#endif

/* ======================================================================
 * Types
 * ====================================================================== */

/** Called after a parameter value is changed via param_table_set(). */
typedef void (*param_change_fn)(void *ctx, const char *key,
                                const char *old_val, const char *new_val);

/** Single parameter entry (internal — do not manipulate directly). */
typedef struct {
    const char       *key;                   /* pointer to caller-owned string */
    char              value[PARAM_VALUE_SIZE];
    bool              readonly;
    param_change_fn   on_change;             /* may be NULL */
    void             *change_ctx;            /* opaque context for on_change   */
} param_entry_t;

/** Parameter table instance. */
typedef struct {
    param_entry_t  entries[PARAM_MAX_ENTRIES];
    uint8_t        count;                    /* current number of entries      */
} param_table_t;

/* ======================================================================
 * API
 * ====================================================================== */

/** Zero-initialise a param table. @thread_safe: no */
param_err_t param_table_init(param_table_t *pt);

/** Define a new parameter with a default value.
 *  The key is kept by pointer, not copied, and is read again by every call
 *  that takes a key and by param_table_list(). It must stay valid and
 *  unchanged for as long as pt is in use: a string literal or a static array.
 *  A stack buffer, or one buffer reused for several keys, gives wrong or dead
 *  keys and no error. @thread_safe: no */
param_err_t param_table_define(param_table_t *pt, const char *key,
                               const char *default_val, bool readonly);

/** Set a parameter value by key. Rejects readonly params. @thread_safe: no */
param_err_t param_table_set(param_table_t *pt, const char *key,
                            const char *value);

/** Get a parameter value by key into caller buffer. @thread_safe: no */
param_err_t param_table_get(const param_table_t *pt, const char *key,
                            char *value, uint16_t size);

/** Attach a change callback to an existing parameter. @thread_safe: no */
param_err_t param_table_set_callback(param_table_t *pt, const char *key,
                                     param_change_fn fn, void *ctx);

/** Return index of key, or -1 if not found. @thread_safe: yes (pure read) */
int param_table_find(const param_table_t *pt, const char *key);

/** Return current entry count. @thread_safe: yes (pure read) */
uint8_t param_table_count(const param_table_t *pt);

/**
 * Format all parameters as "key=value\n" lines into buf.
 * Returns number of bytes written (excluding NUL), or -1 on error.
 * @thread_safe: no
 */
int param_table_list(const param_table_t *pt, char *buf, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* PARAM_TABLE_H */
