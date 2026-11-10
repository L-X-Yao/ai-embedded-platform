// SPDX-License-Identifier: MIT
/*
 * boot_counter -- persistent boot count and reset reason tracker
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none -- storage via read/write callbacks)
 * Instances: multi
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

#define BOOT_COUNTER_MAGIC 0xB0071C05u

typedef struct {
    uint32_t boot_count;    /* cumulative boot count */
    uint32_t reset_reason;  /* last reset reason code (MCU-specific) */
    uint32_t magic;         /* BOOT_COUNTER_MAGIC */
    uint32_t seq;           /* tells two copies apart; 0 with one copy */
    uint32_t crc;           /* CRC-32 of the fields above */
} boot_counter_record_t;

/* ---------- Runtime context ---------- */

typedef struct {
    boot_counter_storage_t storage;         /* injected callbacks */
    boot_counter_record_t  record;          /* cached persistent data */
    uint32_t               uptime_seconds;  /* runtime uptime counter */
    bool                   loaded;          /* true after successful load */
    boot_counter_storage_read_fn  read2;    /* second copy, NULL if none */
    boot_counter_storage_write_fn write2;
    uint32_t               stored_seq;      /* seq of the newest stored record */
    bool                   stored_in_2;     /* that record is the second copy */
} boot_counter_t;

/* ---------- API ---------- */

/** @thread_safe: no (caller serializes) */
boot_counter_err_t boot_counter_init(boot_counter_t *ctx,
                                     const boot_counter_storage_t *storage);

/** @thread_safe: no (caller serializes)
 *  Keep a second copy of the record, so that power lost during a write cannot
 *  take the count with it. read2 and write2 work like the storage callbacks
 *  and get the same user_ctx, but on storage a write to the first copy never
 *  touches: another flash sector, not the same one. Records alternate between
 *  the two, and load takes the newest that passes its checks.
 *  Call it between boot_counter_init() and boot_counter_load();
 *  BOOT_COUNTER_ERR_PARAM once a load has succeeded. */
boot_counter_err_t boot_counter_set_second_copy(boot_counter_t *ctx,
                                                boot_counter_storage_read_fn read2,
                                                boot_counter_storage_write_fn write2);

/** @thread_safe: no (caller serializes)
 *  Load record from persistent storage into ctx. A record that fails its
 *  magic or CRC, as erased flash does, is no record: the count starts from
 *  zero and this returns BOOT_COUNTER_OK. With one copy that is also what a
 *  backend gets if it loses power between erase and program, so the count
 *  is lost: boot_counter_set_second_copy() is what keeps it. */
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
