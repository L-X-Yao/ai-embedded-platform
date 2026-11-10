// SPDX-License-Identifier: MIT
/*
 * ota_ab — A/B firmware slot manager with rollback
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      flash_hal
 * Instances: multi
 */

#ifndef OTA_AB_H
#define OTA_AB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    OTA_AB_OK         =  0,
    OTA_AB_ERR_NULL   = -1,
    OTA_AB_ERR_FLASH  = -2,
    OTA_AB_ERR_CRC    = -3,
    OTA_AB_ERR_PARAM = -4,
    OTA_AB_ERR_SIZE   = -5,
} ota_ab_err_t;

/* ---------- OTA state flags ---------- */

#define OTA_AB_FLAG_IDLE       0x00u
#define OTA_AB_FLAG_PENDING    0x01u
#define OTA_AB_FLAG_SUCCESS    0x02u

/* ---------- slot layout ---------- */

typedef struct {
    uint32_t slot_a_addr;
    uint32_t slot_b_addr;
    uint32_t slot_max_size;
    uint32_t params_addr;
    uint32_t params_size;
    uint8_t  max_boot_count;   /* 1..254; ota_ab_init refuses 0 and 255 */
    /* Second params region, params_size long, in an erase unit of its own:
     * erasing one region must not touch the other. Records then alternate
     * between the two, so a power cut during a write leaves the last good
     * record. ota_ab_init refuses regions that overlap. 0 keeps one copy at
     * params_addr, and power lost between its erase and write loses the
     * record: the next init falls back to slot 0 with no trial state. */
    uint32_t params_addr2;
} ota_ab_layout_t;

/* ---------- persistent parameters ---------- */

#define OTA_AB_PARAMS_MAGIC 0xAB12CD34u

typedef struct {
    uint32_t magic;
    uint8_t  active_slot;
    uint8_t  ota_flag;
    uint8_t  boot_count;
    uint8_t  seq;              /* tells two copies apart; 0 with one copy */
    uint32_t fw_size;
    uint32_t fw_crc;
    uint32_t crc32;
} ota_ab_params_t;

/* ---------- flash HAL (caller provides) ---------- */

typedef struct {
    bool (*erase)(void *ctx, uint32_t addr, uint32_t size);
    bool (*write)(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len);
    bool (*read)(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len);
    void *ctx;
} ota_ab_flash_t;

/* ---------- manager state ---------- */

typedef struct {
    const ota_ab_flash_t  *flash;
    ota_ab_layout_t        layout;
    ota_ab_params_t        params;
    uint8_t                stored_seq;   /* seq of the newest record on flash */
    bool                   stored_in_2;  /* that record is at params_addr2 */
} ota_ab_t;

/* ---------- API ---------- */

/** Initialize and load params from flash. @thread_safe: no */
ota_ab_err_t ota_ab_init(ota_ab_t *m, const ota_ab_flash_t *flash,
                         const ota_ab_layout_t *layout);

/** Get the currently active slot (0=A, 1=B). @thread_safe: caller_must_lock */
uint8_t ota_ab_active_slot(const ota_ab_t *m);

/** Get the target slot for next OTA (inactive one). @thread_safe: caller_must_lock */
uint8_t ota_ab_target_slot(const ota_ab_t *m);

/** Get slot base address. @thread_safe: yes (pure) */
uint32_t ota_ab_slot_addr(const ota_ab_t *m, uint8_t slot);

/** Mark OTA as pending (after download + CRC verify). @thread_safe: caller_must_lock */
ota_ab_err_t ota_ab_mark_pending(ota_ab_t *m, uint32_t fw_size, uint32_t fw_crc);

/** Called by bootloader: process boot event (advance state machine). @thread_safe: no */
ota_ab_err_t ota_ab_boot_event(ota_ab_t *m);

/** Called by app: confirm current firmware is good. @thread_safe: caller_must_lock */
ota_ab_err_t ota_ab_confirm(ota_ab_t *m);

/** Compute CRC32 of flash region. @thread_safe: yes (pure) */
uint32_t ota_ab_crc32(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* OTA_AB_H */
