// SPDX-License-Identifier: MIT
/*
 * prop_flash_ring.c — property-based tests for flash_ring
 *
 * Build: add to test/CMakeLists.txt or compile standalone:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_flash_ring.c ../flash_ring.c -o prop_flash_ring
 */

#include "test_harness.h"
#include "property_test.h"
#include "../flash_ring.h"
#include <string.h>

/* ---------- mock flash ---------- */

#define MOCK_SIZE    (32 * 1024)
#define MOCK_SECTOR  4096
#define MOCK_SLOT    64
#define MOCK_HDR     0
#define MOCK_DATA    MOCK_SECTOR
#define MOCK_SLOTS   ((MOCK_SIZE - MOCK_SECTOR) / MOCK_SLOT)

static uint8_t g_flash[MOCK_SIZE];
static uint32_t g_overprogram;     /* bytes programmed over cells not erased */

static int m_erase(void *c, uint32_t a) {
    (void)c;
    uint32_t e = a + MOCK_SECTOR;
    if (e > MOCK_SIZE) e = MOCK_SIZE;
    memset(&g_flash[a], 0xFF, e - a);
    return 0;
}
static int m_write(void *c, uint32_t a, const uint8_t *d, uint16_t l) {
    (void)c;
    if (a + l > MOCK_SIZE) return -1;
    for (uint16_t i = 0; i < l; i++) {   /* NOR: a program only clears bits */
        if (g_flash[a + i] != 0xFF) g_overprogram++;
        g_flash[a + i] &= d[i];
    }
    return 0;
}
static int m_read(void *c, uint32_t a, uint8_t *b, uint16_t l) {
    (void)c;
    if (a + l > MOCK_SIZE) return -1;
    memcpy(b, &g_flash[a], l);
    return 0;
}

static const flash_ring_io_t g_io = {
    .erase_sector = m_erase, .write = m_write, .read = m_read, .ctx = NULL,
};
static const flash_ring_cfg_t g_cfg = {
    .slot_size = MOCK_SLOT, .total_slots = MOCK_SLOTS,
    .sector_size = MOCK_SECTOR, .header_addr = MOCK_HDR,
    .data_start_addr = MOCK_DATA, .persist_interval = 8,
};

/* Property: count is always consistent after random push/pop sequences */
PROPERTY(count_invariant, 10000) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    uint32_t expected = 0;
    int ops = ARBI_RANGE(seed, 1, 100);
    for (int i = 0; i < ops; i++) {
        if (ARBI_UINT8(seed) % 3 != 0 && expected < MOCK_SLOTS - 1) {
            uint8_t frame[MOCK_SLOT];
            ARBI_FILL(seed, frame, MOCK_SLOT);
            if (flash_ring_push(&ring, frame, MOCK_SLOT) == FLASH_RING_OK)
                expected++;
        } else if (expected > 0) {
            if (flash_ring_pop(&ring) == FLASH_RING_OK)
                expected--;
        }
        PROP_ASSERT_EQ(flash_ring_count(&ring), expected);
    }
}

/* Property: push then peek returns identical data */
PROPERTY(push_peek_roundtrip, 10000) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    uint8_t frame[MOCK_SLOT];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, MOCK_SLOT);
    ARBI_FILL(seed, frame, len);

    uint32_t idx = ring.write_idx;
    PROP_ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, len));

    uint8_t buf[MOCK_SLOT];
    memset(buf, 0xCC, sizeof(buf));
    PROP_ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, idx, buf, len));
    PROP_ASSERT_MEM_EQ(buf, frame, len);
}

/* Property: discard_all always yields count=0 */
PROPERTY(discard_resets, 5000) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    int pushes = ARBI_RANGE(seed, 0, 50);
    uint8_t frame[MOCK_SLOT];
    for (int i = 0; i < pushes; i++) {
        ARBI_FILL(seed, frame, MOCK_SLOT);
        flash_ring_push(&ring, frame, MOCK_SLOT);
    }

    flash_ring_discard_all(&ring);
    PROP_ASSERT_EQ(0u, flash_ring_count(&ring));
    PROP_ASSERT_EQ(FLASH_RING_ERR_EMPTY, flash_ring_pop(&ring));
}

/* Property: ring survives re-init (header persisted) */
PROPERTY(reinit_preserves_state, 5000) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    int pushes = ARBI_RANGE(seed, 1, 20);
    uint8_t frame[MOCK_SLOT];
    for (int i = 0; i < pushes; i++) {
        ARBI_FILL(seed, frame, MOCK_SLOT);
        flash_ring_push(&ring, frame, MOCK_SLOT);
    }
    flash_ring_flush_header(&ring);
    uint32_t count_before = flash_ring_count(&ring);

    flash_ring_t ring2;
    flash_ring_init(&ring2, &g_io, &g_cfg);
    PROP_ASSERT_EQ(flash_ring_count(&ring2), count_before);
}

/* Property: over many laps frames come out whole, in order, none programmed
 * over, and the newest frame is never lost while it is still unread. */
PROPERTY(random_laps_keep_order, 2000) {
    static const flash_ring_cfg_t small = {
        .slot_size = 1024, .total_slots = 14, .sector_size = MOCK_SECTOR,
        .header_addr = MOCK_HDR, .data_start_addr = MOCK_DATA,
        .persist_interval = 5,
    };
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_overprogram = 0;
    flash_ring_t ring;
    PROP_ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_io, &small));

    uint32_t next = 1, newest = 0, last_read = 0;
    int ops = ARBI_RANGE(seed, 50, 200);
    for (int i = 0; i < ops; i++) {
        uint32_t frame[2];
        if (ARBI_UINT8(seed) % 4 != 0) {
            frame[0] = next;
            frame[1] = ~next;
            PROP_ASSERT_EQ(FLASH_RING_OK,
                           flash_ring_push(&ring, (const uint8_t *)frame,
                                           (uint16_t)sizeof(frame)));
            newest = next++;
        } else if (flash_ring_count(&ring) > 0) {
            PROP_ASSERT_EQ(FLASH_RING_OK,
                           flash_ring_peek(&ring, ring.read_idx, (uint8_t *)frame,
                                           (uint16_t)sizeof(frame)));
            PROP_ASSERT_EQ(frame[1], ~frame[0]);
            PROP_ASSERT(frame[0] > last_read);
            last_read = frame[0];
            PROP_ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
        }
        PROP_ASSERT(flash_ring_count(&ring) < small.total_slots);
        PROP_ASSERT((flash_ring_count(&ring) > 0) == (newest > last_read));
    }
    PROP_ASSERT_EQ(0u, g_overprogram);
}

TEST_MAIN("flash_ring properties")
    RUN(count_invariant);
    RUN(push_peek_roundtrip);
    RUN(discard_resets);
    RUN(reinit_preserves_state);
    RUN(random_laps_keep_order);
TEST_MAIN_END()
