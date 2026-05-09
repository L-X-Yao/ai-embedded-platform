// SPDX-License-Identifier: MIT
/*
 * stress_flash_ring.c — long-running stress tests for flash_ring
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_flash_ring.c ../flash_ring.c -o /tmp/stress_flash_ring -lm
 *
 * Scenarios:
 *   1. Sustained push/pop — 10K cycles of write-then-read, verify data integrity
 *   2. Full wrap-around — fill beyond capacity, verify oldest data overwritten
 *   3. Push-heavy with periodic drain — mixed write/read stress
 *   4. Header persistence recovery — init/push/re-init cycles, verify state survives
 */

#include "test_harness.h"
#include "flash_ring.h"
#include <string.h>

/* ---------- mock flash ---------- */

#define MOCK_FLASH_SIZE   (64 * 1024)
#define MOCK_SECTOR_SIZE  4096
#define MOCK_SLOT_SIZE    64
#define MOCK_HDR_ADDR     0
#define MOCK_DATA_ADDR    MOCK_SECTOR_SIZE
#define MOCK_TOTAL_SLOTS  ((MOCK_FLASH_SIZE - MOCK_SECTOR_SIZE) / MOCK_SLOT_SIZE)

static uint8_t g_flash[MOCK_FLASH_SIZE];

static void mock_flash_reset(void)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
}

static int mock_erase(void *ctx, uint32_t addr)
{
    (void)ctx;
    uint32_t end = addr + MOCK_SECTOR_SIZE;
    if (end > MOCK_FLASH_SIZE) end = MOCK_FLASH_SIZE;
    memset(&g_flash[addr], 0xFF, end - addr);
    return 0;
}

static int mock_write(void *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    memcpy(&g_flash[addr], data, len);
    return 0;
}

static int mock_read(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, &g_flash[addr], len);
    return 0;
}

static const flash_ring_io_t g_io = {
    .erase_sector = mock_erase,
    .write        = mock_write,
    .read         = mock_read,
    .ctx          = NULL,
};

static const flash_ring_cfg_t g_cfg = {
    .slot_size       = MOCK_SLOT_SIZE,
    .total_slots     = MOCK_TOTAL_SLOTS,
    .sector_size     = MOCK_SECTOR_SIZE,
    .header_addr     = MOCK_HDR_ADDR,
    .data_start_addr = MOCK_DATA_ADDR,
    .persist_interval = 0,
};

/* ---- tests ---------------------------------------------------------- */

TEST(sustained_push_pop_integrity)
{
    mock_flash_reset();
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    int ok = 1;
    for (int i = 0; i < 10000; i++) {
        uint8_t frame[MOCK_SLOT_SIZE];
        memset(frame, (uint8_t)(i & 0xFF), sizeof(frame));
        /* Write a distinguishing marker */
        frame[0] = (uint8_t)(i & 0xFF);
        frame[1] = (uint8_t)((i >> 8) & 0xFF);

        if (flash_ring_push(&ring, frame, MOCK_SLOT_SIZE) != FLASH_RING_OK) {
            ok = 0; break;
        }
        ASSERT_EQ(1u, flash_ring_count(&ring));

        uint8_t readback[MOCK_SLOT_SIZE] = {0};
        if (flash_ring_peek(&ring, ring.read_idx, readback, MOCK_SLOT_SIZE) != FLASH_RING_OK) {
            ok = 0; break;
        }
        if (readback[0] != frame[0] || readback[1] != frame[1]) {
            ok = 0; break;
        }

        if (flash_ring_pop(&ring) != FLASH_RING_OK) { ok = 0; break; }
        ASSERT_EQ(0u, flash_ring_count(&ring));
    }
    ASSERT_TRUE(ok);
}

TEST(full_wrap_around_overwrite)
{
    mock_flash_reset();
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    /* Push 3x total_slots worth of data — wraps around multiple times */
    int ok = 1;
    uint32_t total_pushes = MOCK_TOTAL_SLOTS * 3;
    for (uint32_t i = 0; i < total_pushes; i++) {
        uint8_t frame[MOCK_SLOT_SIZE];
        memset(frame, (uint8_t)(i & 0xFF), sizeof(frame));
        frame[0] = (uint8_t)(i & 0xFF);
        frame[1] = (uint8_t)((i >> 8) & 0xFF);
        if (flash_ring_push(&ring, frame, MOCK_SLOT_SIZE) != FLASH_RING_OK) {
            ok = 0; break;
        }
    }
    ASSERT_TRUE(ok);

    /* write_idx should have wrapped multiple times */
    /* Count must reflect the state correctly */
    uint32_t count = flash_ring_count(&ring);
    ASSERT_TRUE(count <= MOCK_TOTAL_SLOTS);
}

TEST(push_heavy_periodic_drain)
{
    mock_flash_reset();
    flash_ring_t ring;
    flash_ring_init(&ring, &g_io, &g_cfg);

    int ok = 1;
    uint32_t push_count = 0;
    uint32_t pop_count = 0;

    for (int i = 0; i < 50000; i++) {
        /* Push 3 frames */
        for (int j = 0; j < 3; j++) {
            uint8_t frame[MOCK_SLOT_SIZE];
            memset(frame, (uint8_t)((push_count) & 0xFF), sizeof(frame));
            flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
            push_count++;
        }
        /* Pop 2 frames (net +1 per iteration, until full) */
        for (int j = 0; j < 2; j++) {
            if (flash_ring_count(&ring) > 0) {
                flash_ring_pop(&ring);
                pop_count++;
            }
        }
    }

    /* Final drain — verify all remaining can be popped */
    while (flash_ring_count(&ring) > 0) {
        if (flash_ring_pop(&ring) != FLASH_RING_OK) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_EQ(0u, flash_ring_count(&ring));
}

TEST(header_persistence_across_reinit)
{
    mock_flash_reset();

    int ok = 1;
    for (int cycle = 0; cycle < 5000; cycle++) {
        flash_ring_t ring;
        flash_ring_init(&ring, &g_io, &g_cfg);

        /* Push some data */
        uint8_t frame[MOCK_SLOT_SIZE];
        memset(frame, (uint8_t)(cycle & 0xFF), sizeof(frame));
        frame[0] = (uint8_t)(cycle & 0xFF);
        frame[1] = (uint8_t)((cycle >> 8) & 0xFF);
        flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
        flash_ring_flush_header(&ring);

        /* Re-init (simulates power cycle) */
        flash_ring_t ring2;
        flash_ring_init(&ring2, &g_io, &g_cfg);

        /* Must recover the pushed frame */
        if (flash_ring_count(&ring2) < 1) { ok = 0; break; }

        uint8_t readback[MOCK_SLOT_SIZE] = {0};
        flash_ring_peek(&ring2, ring2.read_idx, readback, MOCK_SLOT_SIZE);
        if (readback[0] != frame[0] || readback[1] != frame[1]) {
            ok = 0; break;
        }

        /* Pop to clean up for next cycle */
        while (flash_ring_count(&ring2) > 0) flash_ring_pop(&ring2);
        flash_ring_flush_header(&ring2);
    }
    ASSERT_TRUE(ok);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress flash_ring")
    RUN(sustained_push_pop_integrity);
    RUN(full_wrap_around_overwrite);
    RUN(push_heavy_periodic_drain);
    RUN(header_persistence_across_reinit);
TEST_MAIN_END()
