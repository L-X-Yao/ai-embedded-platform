/*
 * config_store -- configuration management with defaults, validation,
 *                 and storage callbacks
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- self-contained)
 *
 * Passive design: application defines typed config entries with defaults
 * and validation ranges at init time, then gets/sets values with
 * automatic type and range checking.  Persistence via save/load
 * callbacks -- application decides when and where (kvs/flash/EEPROM).
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef CONFIG_STORE_MAX_ENTRIES
#define CONFIG_STORE_MAX_ENTRIES     16
#endif

#ifndef CONFIG_STORE_STRING_SIZE
#define CONFIG_STORE_STRING_SIZE     32
#endif

/* ---------- Error codes ---------- */

typedef enum {
    CONFIG_STORE_OK             =  0,
    CONFIG_STORE_ERR_PARAMS     = -1,
    CONFIG_STORE_ERR_FULL       = -2,
    CONFIG_STORE_ERR_NOT_FOUND  = -3,
    CONFIG_STORE_ERR_TYPE       = -4,
    CONFIG_STORE_ERR_RANGE      = -5,
    CONFIG_STORE_ERR_STORAGE    = -6,
} config_store_err_t;

/* ---------- Value types ---------- */

typedef enum {
    CONFIG_TYPE_INT32,
    CONFIG_TYPE_UINT32,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_BOOL,
} config_type_t;

typedef union {
    int32_t  i32;
    uint32_t u32;
    float    f32;
    bool     b;
    char     str[CONFIG_STORE_STRING_SIZE];
} config_value_t;

/* ---------- Entry ---------- */

typedef struct {
    const char     *key;
    config_type_t   type;
    config_value_t  default_val;
    config_value_t  min_val;        /* for numeric types */
    config_value_t  max_val;        /* for numeric types */
    config_value_t  current;
    bool            modified;       /* true if different from default */
} config_entry_t;

/* ---------- Storage abstraction ---------- */

typedef struct {
    config_store_err_t (*save)(void *ctx, const char *key,
                               const uint8_t *data, uint16_t len);
    config_store_err_t (*load)(void *ctx, const char *key,
                               uint8_t *data, uint16_t len);
    void *ctx;
} config_store_storage_t;

/* ---------- Store context ---------- */

typedef struct {
    config_entry_t               entries[CONFIG_STORE_MAX_ENTRIES];
    uint8_t                      count;
    const config_store_storage_t *storage;
} config_store_t;

/* ---------- API ---------- */

/** Initialize store.  storage may be NULL (no persistence). */
config_store_err_t config_store_init(config_store_t *cs,
                                     const config_store_storage_t *storage);

/* --- Define entries (call at startup) --- */

config_store_err_t config_store_define_int(config_store_t *cs,
                                           const char *key,
                                           int32_t default_val,
                                           int32_t min, int32_t max);

config_store_err_t config_store_define_uint(config_store_t *cs,
                                            const char *key,
                                            uint32_t default_val,
                                            uint32_t min, uint32_t max);

config_store_err_t config_store_define_float(config_store_t *cs,
                                             const char *key,
                                             float default_val,
                                             float min, float max);

config_store_err_t config_store_define_bool(config_store_t *cs,
                                            const char *key,
                                            bool default_val);

config_store_err_t config_store_define_string(config_store_t *cs,
                                              const char *key,
                                              const char *default_val);

/* --- Set values (with type + range validation) --- */

config_store_err_t config_store_set_int(config_store_t *cs,
                                        const char *key, int32_t val);

config_store_err_t config_store_set_uint(config_store_t *cs,
                                         const char *key, uint32_t val);

config_store_err_t config_store_set_float(config_store_t *cs,
                                          const char *key, float val);

config_store_err_t config_store_set_bool(config_store_t *cs,
                                         const char *key, bool val);

config_store_err_t config_store_set_string(config_store_t *cs,
                                           const char *key,
                                           const char *val);

/* --- Get values --- */

config_store_err_t config_store_get_int(const config_store_t *cs,
                                        const char *key, int32_t *val);

config_store_err_t config_store_get_uint(const config_store_t *cs,
                                         const char *key, uint32_t *val);

config_store_err_t config_store_get_float(const config_store_t *cs,
                                          const char *key, float *val);

config_store_err_t config_store_get_bool(const config_store_t *cs,
                                         const char *key, bool *val);

config_store_err_t config_store_get_string(const config_store_t *cs,
                                           const char *key, char *val,
                                           uint16_t size);

/* --- Persistence --- */

/** Save all modified entries via storage callbacks. */
config_store_err_t config_store_save_all(config_store_t *cs);

/** Load all entries via storage callbacks. */
config_store_err_t config_store_load_all(config_store_t *cs);

/** Reset all entries to their default values. */
config_store_err_t config_store_reset_defaults(config_store_t *cs);

/** Return number of defined entries. */
uint8_t config_store_count(const config_store_t *cs);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_STORE_H */
