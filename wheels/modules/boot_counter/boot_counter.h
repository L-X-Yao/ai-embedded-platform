// SPDX-License-Identifier: MIT
/*
 * boot_counter -- persistent boot count and reset reason tracker
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- storage via read/write callbacks)
 *
 * Passive design: application provides storage callbacks (can wire to
 * kvs/flash/eeprom). Tracks boot count, reset reason, and uptime.
 * Call tick_second() from a 1-second timer to increment uptime.
 */

#ifndef BOOT_COUNTER_H
#define BOOT_COUNTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Error codes ---------- */

typedef enum {
    BOOT_COUNTER_OK            =  0,
    BOOT_COUNTER_ERR_PARAM     = -1,
    BOOT_COUNTER_ERR_STORAGE   = -2,
} boot_counter_err_t;

/* ---------- Storage callbacks ---------- */

/**
 * Read persistent data.
 * @param buf      destination buffer
 * @param size     bytes to read
 * @param user_ctx opaque pointer passed through from init
 * @return 0 on success, non-zero on failure
 */
typedef int (*boot_counter_storage_read_fn)(uint8_t *buf, size_t size,
                                            void *user_ctx);

/**
 * Write persistent data.
 * @param buf      source buffer
 * @param size     bytes to write
 * @param user_ctx opaque pointer passed through from init
 * @return 0 on success, non-zero on failure
 */
typedef int (*boot_counter_storage_write_fn)(const uint8_t *buf, size_t size,
                                             void *user_ctx);

typedef struct {
    boot_counter_storage_read_fn  read;
    boot_counter_storage_write_fn write;
    void                         *user_ctx;
} boot_counter_storage_t;

/* ---------- Persistent record (serialized to storage) ---------- */

typedef struct {
    uint32_t boot_count;    /* cumulative boot count */
    uint32_t reset_reason;  /* last reset reason code (MCU-specific) */
} boot_counter_record_t;

/* ---------- Runtime context ---------- */

typedef struct {
    boot_counter_storage_t storage;         /* injected callbacks */
    boot_counter_record_t  record;          /* cached persistent data */
    uint32_t               uptime_seconds;  /* runtime uptime counter */
    bool                   loaded;          /* true after successful load */
} boot_counter_t;

/* ---------- API ---------- */

/** @thread_safe: no (caller serializes) */
boot_counter_err_t boot_counter_init(boot_counter_t *ctx,
                                     const boot_counter_storage_t *storage);

/** @thread_safe: no (caller serializes)
 *  Load record from persistent storage into ctx. */
boot_counter_err_t boot_counter_load(boot_counter_t *ctx);

/** @thread_safe: no (caller serializes)
 *  Increment boot count and save to persistent storage. */
boot_counter_err_t boot_counter_increment(boot_counter_t *ctx);

/** @thread_safe: yes (read-only, atomic uint32_t) */
uint32_t boot_counter_get_count(const boot_counter_t *ctx);

/** @thread_safe: yes (read-only, atomic uint32_t) */
uint32_t boot_counter_get_reset_reason(const boot_counter_t *ctx);

/** @thread_safe: no (caller serializes)
 *  Set reset reason and save to persistent storage. */
boot_counter_err_t boot_counter_set_reset_reason(boot_counter_t *ctx,
                                                  uint32_t reason);

/** @thread_safe: yes (read-only, atomic uint32_t) */
uint32_t boot_counter_get_uptime_seconds(const boot_counter_t *ctx);

/** @thread_safe: no (caller serializes)
 *  Call once per second to increment uptime counter. */
void boot_counter_tick_second(boot_counter_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_COUNTER_H */
