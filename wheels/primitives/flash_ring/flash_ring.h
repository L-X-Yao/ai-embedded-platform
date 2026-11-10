// SPDX-License-Identifier: MIT
/*
 * flash_ring — Flash-backed circular FIFO with persistent header
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef FLASH_RING_H
#define FLASH_RING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    FLASH_RING_OK        =  0,
    FLASH_RING_ERR_PARAM = -1,
    FLASH_RING_ERR_IO    = -2,
    FLASH_RING_ERR_EMPTY = -3,
} flash_ring_err_t;

/* ---------- flash HAL injection ---------- */

typedef struct {
    int (*erase_sector)(void *ctx, uint32_t addr);
    int (*write)(void *ctx, uint32_t addr, const uint8_t *data, uint16_t len);
    int (*read)(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len);
    void *ctx;
} flash_ring_io_t;

/* ---------- configuration ---------- */

typedef struct {
    uint16_t slot_size;         /* bytes per frame slot */
    uint32_t total_slots;       /* total number of frame slots */
    uint32_t sector_size;       /* flash sector size in bytes */
    uint32_t header_addr;       /* address of persistent header sector */
    uint32_t data_start_addr;   /* address where data slots begin */
    uint16_t persist_interval;  /* flush header every N writes (0 = every write) */
} flash_ring_cfg_t;

/* ---------- persistent header (on-flash format) ---------- */

#define FLASH_RING_MAGIC 0xFB0EFEEDu

/* ---------- instance ---------- */

typedef struct {
    const flash_ring_io_t *io;        /* internal */
    flash_ring_cfg_t       cfg;       /* internal */
    uint32_t               write_idx; /* internal */
    uint32_t               read_idx;  /* read-only for callers: slot of the oldest frame */
    uint32_t               frame_count; /* internal: monotonic total writes */
    uint16_t               since_flush; /* internal */
} flash_ring_t;

/* ---------- API ---------- */

/** @thread_safe: no (caller must serialize) @isr_safe: no */
flash_ring_err_t flash_ring_init(flash_ring_t *ring,
                                 const flash_ring_io_t *io,
                                 const flash_ring_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
flash_ring_err_t flash_ring_push(flash_ring_t *ring,
                                 const uint8_t *frame, uint16_t len);

/**
 * Read the frame in slot idx without consuming it. idx is an absolute slot:
 * the oldest frame is in ring->read_idx and the next one in
 * (ring->read_idx + 1) % total_slots.
 * @thread_safe: no @isr_safe: no
 */
flash_ring_err_t flash_ring_peek(const flash_ring_t *ring, uint32_t idx,
                                 uint8_t *buf, uint16_t len);

/** @thread_safe: no @isr_safe: no */
flash_ring_err_t flash_ring_pop(flash_ring_t *ring);

/** @thread_safe: no @isr_safe: no */
uint32_t flash_ring_count(const flash_ring_t *ring);

/** @thread_safe: no @isr_safe: no */
flash_ring_err_t flash_ring_discard_all(flash_ring_t *ring);

/** @thread_safe: no @isr_safe: no */
flash_ring_err_t flash_ring_flush_header(flash_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_RING_H */
