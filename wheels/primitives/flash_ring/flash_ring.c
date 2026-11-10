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

/*
 * The writer keeps one rule: after every completed push, and after init, the
 * slot at write_idx and the rest of its sector are blank. So it erases the next
 * sector just before it programs the last slot of the current one. That gives
 * init a blank slot to look for, and it means write_idx == read_idx only ever
 * says the ring is empty.
 */

#define ERASED_BYTE 0xFFu
#define SCAN_CHUNK  32u

static uint32_t slots_per_sector(const flash_ring_t *ring)
{
    return ring->cfg.sector_size / ring->cfg.slot_size;
}

static bool same_sector(const flash_ring_t *ring, uint32_t a, uint32_t b)
{
    return a / slots_per_sector(ring) == b / slots_per_sector(ring);
}

/* First slot of the sector after the one holding idx, wrapping to 0. */
static uint32_t next_sector_start(const flash_ring_t *ring, uint32_t idx)
{
    uint32_t sps  = slots_per_sector(ring);
    uint32_t next = (idx / sps + 1u) * sps;
    return next < ring->cfg.total_slots ? next : 0u;
}

static bool last_in_sector(const flash_ring_t *ring, uint32_t idx)
{
    return idx + 1u == ring->cfg.total_slots ||
           (idx + 1u) % slots_per_sector(ring) == 0u;
}

/* The next sector is about to go: its frames are lost, so the reader skips them. */
static void drop_next_sector(flash_ring_t *ring)
{
    uint32_t next = next_sector_start(ring, ring->write_idx);
    if (same_sector(ring, ring->read_idx, next)) {
        ring->read_idx = next_sector_start(ring, next);
    }
}

/* One more change the header on flash does not show. It must not wrap: a
 * flush that keeps failing has to stay due. */
static void count_unflushed(flash_ring_t *ring)
{
    if (ring->since_flush < UINT16_MAX) {
        ring->since_flush++;
    }
}

/* The index step of a push, shared with init so the two cannot disagree. */
static void advance_write(flash_ring_t *ring)
{
    if (last_in_sector(ring, ring->write_idx)) {
        drop_next_sector(ring);
    }
    ring->write_idx = (ring->write_idx + 1u) % ring->cfg.total_slots;
    ring->frame_count++;
    count_unflushed(ring);
}

/*
 * Init walks forward from the stored write_idx, so that slot must still hold
 * what the writer put there. The writer erases its sector again after at most
 * total_slots - slots_per_sector pushes, which caps the interval.
 */
static bool flush_due(const flash_ring_t *ring)
{
    uint32_t limit    = ring->cfg.total_slots - slots_per_sector(ring);
    uint16_t interval = ring->cfg.persist_interval;

    if (interval == 0u) {
        return true;
    }
    return ring->since_flush >= (interval < limit ? interval : limit);
}

/* Is everything from slot idx to the end of its sector erased? */
static flash_ring_err_t tail_blank(const flash_ring_t *ring, uint32_t idx,
                                   bool *blank)
{
    const flash_ring_io_t *io = ring->io;
    uint32_t sps  = slots_per_sector(ring);
    uint32_t last = (idx / sps + 1u) * sps;
    if (last > ring->cfg.total_slots) {
        last = ring->cfg.total_slots;
    }

    uint32_t addr = slot_addr(ring, idx);
    uint32_t end  = slot_addr(ring, last);
    uint8_t  chunk[SCAN_CHUNK];

    while (addr < end) {
        uint32_t left = end - addr;
        uint16_t n = (uint16_t)(left < SCAN_CHUNK ? left : SCAN_CHUNK);
        if (io->read(io->ctx, addr, chunk, n) != 0) {
            return FLASH_RING_ERR_IO;
        }
        for (uint16_t i = 0; i < n; i++) {
            if (chunk[i] != ERASED_BYTE) {
                *blank = false;
                return FLASH_RING_OK;
            }
        }
        addr += n;
    }
    *blank = true;
    return FLASH_RING_OK;
}

/*
 * The header is up to persist_interval pushes old. Walk forward over what was
 * written since, replaying each push, until the rule above holds again. Each
 * step counts towards the next flush, so a ring that resets more often than the
 * interval still rewrites its header in time.
 */
static flash_ring_err_t recover_write_pos(flash_ring_t *ring)
{
    const flash_ring_io_t *io = ring->io;
    uint32_t stored_write = ring->write_idx;
    uint32_t stored_read  = ring->read_idx;
    uint32_t stored_count = ring->frame_count;
    bool blank = false;

    for (uint32_t step = 0; step < ring->cfg.total_slots; step++) {
        flash_ring_err_t err = tail_blank(ring, ring->write_idx, &blank);
        if (err != FLASH_RING_OK) {
            return err;
        }
        if (blank) {
            break;
        }
        advance_write(ring);
    }

    if (!blank) {
        /*
         * No blank tail anywhere, which this code never leaves, but a full ring
         * from the erase-on-entry version can. That code erased a sector on
         * entering it, so a stored write_idx on a sector start sits on the
         * oldest frames, and one part way through a sector means the rest of
         * that sector holds the newest frames and the next one is the oldest.
         * Make the blank sector the writer needs, and record it.
         */
        uint32_t start = stored_write % slots_per_sector(ring) == 0u
                       ? stored_write : next_sector_start(ring, stored_write);

        ring->write_idx   = start;
        ring->frame_count = stored_count;
        ring->read_idx    = stored_read;
        if (stored_read == stored_write) {
            ring->read_idx = start;
        } else if (same_sector(ring, stored_read, start)) {
            ring->read_idx = next_sector_start(ring, start);
        }
        if (io->erase_sector(io->ctx, slot_addr(ring, start)) != 0) {
            return FLASH_RING_ERR_IO;
        }
        return flush_header(ring);
    }

    /* The same version could leave read_idx ahead of the writer in a sector it
     * had just erased: nothing is left there to read. */
    if (same_sector(ring, ring->read_idx, ring->write_idx) &&
        ring->read_idx > ring->write_idx) {
        ring->read_idx = next_sector_start(ring, ring->write_idx);
    }

    /*
     * A cut during push's erase leaves write_idx here on the last slot, with
     * the next sector blank or half erased and no new frame to show for it. No
     * read can tell how far the erase got, and the next push erases that sector
     * anyway, so its frames are given up now.
     */
    if (last_in_sector(ring, ring->write_idx)) {
        drop_next_sector(ring);
    }
    return FLASH_RING_OK;
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
    /* Erasing ahead needs a sector to erase that is not the current one. */
    REQUIRE(cfg->total_slots > cfg->sector_size / cfg->slot_size);
    if (cfg->total_slots <= cfg->sector_size / cfg->slot_size) {
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
        return recover_write_pos(ring);
    }

    ring->write_idx   = 0;
    ring->read_idx    = 0;
    ring->frame_count = 0;
    if (io->erase_sector(io->ctx, cfg->data_start_addr) != 0) {
        return FLASH_RING_ERR_IO;
    }
    return flush_header(ring);
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

    /* Before the last slot, not after it: a cut in between keeps the rule. */
    if (last_in_sector(ring, ring->write_idx)) {
        uint32_t next = next_sector_start(ring, ring->write_idx);
        drop_next_sector(ring);
        if (io->erase_sector(io->ctx, slot_addr(ring, next)) != 0) {
            return FLASH_RING_ERR_IO;
        }
    }

    if (io->write(io->ctx, slot_addr(ring, ring->write_idx), frame, len) != 0) {
        /* The program may have landed anyway, and cells that are not blank
         * cannot be programmed again: leave such a slot behind. */
        bool blank = false;
        if (tail_blank(ring, ring->write_idx, &blank) != FLASH_RING_OK || !blank) {
            advance_write(ring);
        }
        return FLASH_RING_ERR_IO;
    }

    advance_write(ring);

    /* Auto-persist header at configured interval. The frame is stored: a
     * failed flush only leaves the header older and stays due, so report OK
     * rather than have the caller store the frame twice. */
    if (flush_due(ring)) {
        (void)flush_header(ring);
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
    REQUIRE(len <= ring->cfg.slot_size);
    if (idx >= ring->cfg.total_slots || len > ring->cfg.slot_size) {
        return FLASH_RING_ERR_PARAM;
    }

    uint32_t count = flash_ring_count(ring);
    if (count == 0u) {
        return FLASH_RING_ERR_EMPTY;
    }
    /* Distance from the oldest frame, in ring order */
    uint32_t from_read = idx >= ring->read_idx
                       ? idx - ring->read_idx
                       : ring->cfg.total_slots - ring->read_idx + idx;
    if (from_read >= count) {
        return FLASH_RING_ERR_PARAM;
    }

    if (ring->io->read(ring->io->ctx, slot_addr(ring, idx), buf, len) != 0) {
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
    count_unflushed(ring);

    /* The frame is popped: a failed flush must not read as a failed pop. */
    if (flush_due(ring)) {
        (void)flush_header(ring);
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
