// SPDX-License-Identifier: MIT
/*
 * flash_ring — Flash-backed circular FIFO with persistent header
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "flash_ring.h"
#include "contract.h"
#include <string.h>

/* ---------- on-flash header layout ---------- */

typedef struct {
    uint32_t magic;
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t frame_count;
} flash_ring_hdr_t;

/* ---------- internal helpers ---------- */

static flash_ring_err_t flush_header(flash_ring_t *ring)
{
    const flash_ring_io_t *io = ring->io;

    if (io->erase_sector(io->ctx, ring->cfg.header_addr) != 0) {
        return FLASH_RING_ERR_IO;
    }

    flash_ring_hdr_t hdr;
    hdr.magic       = FLASH_RING_MAGIC;
    hdr.write_idx   = ring->write_idx;
    hdr.read_idx    = ring->read_idx;
    hdr.frame_count = ring->frame_count;

    if (io->write(io->ctx, ring->cfg.header_addr,
                  (const uint8_t *)&hdr, (uint16_t)sizeof(hdr)) != 0) {
        return FLASH_RING_ERR_IO;
    }

    ring->since_flush = 0;
    return FLASH_RING_OK;
}

static uint32_t slot_addr(const flash_ring_t *ring, uint32_t idx)
{
    return ring->cfg.data_start_addr + idx * ring->cfg.slot_size;
}

/* ---------- public API ---------- */

flash_ring_err_t flash_ring_init(flash_ring_t *ring,
                                 const flash_ring_io_t *io,
                                 const flash_ring_cfg_t *cfg)
{
    REQUIRE(ring != NULL);
    REQUIRE(io != NULL);
    REQUIRE(cfg != NULL);
    if (ring == NULL || io == NULL || cfg == NULL) {
        return FLASH_RING_ERR_PARAM;
    }
    REQUIRE(io->erase_sector != NULL);
    REQUIRE(io->write != NULL);
    REQUIRE(io->read != NULL);
    if (io->erase_sector == NULL || io->write == NULL || io->read == NULL) {
        return FLASH_RING_ERR_PARAM;
    }
    REQUIRE(cfg->slot_size > 0);
    REQUIRE(cfg->total_slots > 0);
    REQUIRE(cfg->sector_size > 0);
    if (cfg->slot_size == 0 || cfg->total_slots == 0 || cfg->sector_size == 0) {
        return FLASH_RING_ERR_PARAM;
    }
    /* push erases whole sectors: slots must tile them exactly, starting on
     * a sector boundary. This also refuses a slot larger than a sector. */
    REQUIRE(cfg->sector_size % cfg->slot_size == 0);
    REQUIRE(cfg->data_start_addr % cfg->sector_size == 0);
    if (cfg->sector_size % cfg->slot_size != 0 ||
        cfg->data_start_addr % cfg->sector_size != 0) {
        return FLASH_RING_ERR_PARAM;
    }

    ring->io  = io;
    ring->cfg = *cfg;
    ring->since_flush = 0;

    /* Try to load existing header from flash */
    flash_ring_hdr_t hdr;
    if (io->read(io->ctx, cfg->header_addr,
                 (uint8_t *)&hdr, (uint16_t)sizeof(hdr)) != 0) {
        return FLASH_RING_ERR_IO;
    }

    if (hdr.magic == FLASH_RING_MAGIC &&
        hdr.write_idx < cfg->total_slots &&
        hdr.read_idx < cfg->total_slots) {
        ring->write_idx   = hdr.write_idx;
        ring->read_idx    = hdr.read_idx;
        ring->frame_count = hdr.frame_count;
    } else {
        ring->write_idx   = 0;
        ring->read_idx    = 0;
        ring->frame_count = 0;
        return flush_header(ring);
    }

    return FLASH_RING_OK;
}

flash_ring_err_t flash_ring_push(flash_ring_t *ring,
                                 const uint8_t *frame, uint16_t len)
{
    REQUIRE(ring != NULL);
    REQUIRE(ring == NULL || ring->io != NULL);
    if (ring == NULL || ring->io == NULL) {
        return FLASH_RING_ERR_PARAM;
    }
    REQUIRE(frame != NULL);
    REQUIRE(len > 0);
    REQUIRE(len <= ring->cfg.slot_size);
    if (frame == NULL || len == 0 || len > ring->cfg.slot_size) {
        return FLASH_RING_ERR_PARAM;
    }

    const flash_ring_io_t *io = ring->io;
    uint32_t addr = slot_addr(ring, ring->write_idx);

    /* Erase sector if this slot is at a sector boundary */
    uint32_t slots_per_sector = ring->cfg.sector_size / ring->cfg.slot_size;
    if (slots_per_sector > 0 && (ring->write_idx % slots_per_sector) == 0) {
        if (io->erase_sector(io->ctx, addr) != 0) {
            return FLASH_RING_ERR_IO;
        }
    }

    if (io->write(io->ctx, addr, frame, len) != 0) {
        return FLASH_RING_ERR_IO;
    }

    ring->write_idx = (ring->write_idx + 1) % ring->cfg.total_slots;
    ring->frame_count++;
    ring->since_flush++;

    /* Auto-persist header at configured interval */
    uint16_t interval = ring->cfg.persist_interval;
    if (interval == 0 || ring->since_flush >= interval) {
        return flush_header(ring);
    }

    return FLASH_RING_OK;
}

flash_ring_err_t flash_ring_peek(const flash_ring_t *ring, uint32_t idx,
                                 uint8_t *buf, uint16_t len)
{
    REQUIRE(ring != NULL);
    REQUIRE(buf != NULL);
    REQUIRE(len > 0);
    if (ring == NULL || ring->io == NULL || buf == NULL || len == 0) {
        return FLASH_RING_ERR_PARAM;
    }
    REQUIRE(idx < ring->cfg.total_slots);
    if (idx >= ring->cfg.total_slots) {
        return FLASH_RING_ERR_PARAM;
    }

    uint32_t addr = ring->cfg.data_start_addr + idx * ring->cfg.slot_size;
    if (ring->io->read(ring->io->ctx, addr, buf, len) != 0) {
        return FLASH_RING_ERR_IO;
    }
    return FLASH_RING_OK;
}

flash_ring_err_t flash_ring_pop(flash_ring_t *ring)
{
    REQUIRE(ring != NULL);
    if (ring == NULL || ring->io == NULL) {
        return FLASH_RING_ERR_PARAM;
    }
    if (ring->read_idx == ring->write_idx) {
        return FLASH_RING_ERR_EMPTY;
    }

    ring->read_idx = (ring->read_idx + 1) % ring->cfg.total_slots;
    ring->since_flush++;

    uint16_t interval = ring->cfg.persist_interval;
    if (interval == 0 || ring->since_flush >= interval) {
        return flush_header(ring);
    }

    return FLASH_RING_OK;
}

uint32_t flash_ring_count(const flash_ring_t *ring)
{
    REQUIRE(ring != NULL);
    if (ring == NULL) {
        return 0;
    }
    if (ring->write_idx >= ring->read_idx) {
        return ring->write_idx - ring->read_idx;
    }
    return ring->cfg.total_slots - ring->read_idx + ring->write_idx;
}

flash_ring_err_t flash_ring_discard_all(flash_ring_t *ring)
{
    REQUIRE(ring != NULL);
    if (ring == NULL || ring->io == NULL) {
        return FLASH_RING_ERR_PARAM;
    }
    ring->read_idx = ring->write_idx;
    return flush_header(ring);
}

flash_ring_err_t flash_ring_flush_header(flash_ring_t *ring)
{
    REQUIRE(ring != NULL);
    if (ring == NULL || ring->io == NULL) {
        return FLASH_RING_ERR_PARAM;
    }
    return flush_header(ring);
}
