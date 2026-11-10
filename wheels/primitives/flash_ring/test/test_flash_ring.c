// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "flash_ring.h"

/* ---------- mock flash ---------- */

#define MOCK_FLASH_SIZE   (64 * 1024)
#define MOCK_SECTOR_SIZE  4096
#define MOCK_SLOT_SIZE    256
#define MOCK_HDR_ADDR     0
#define MOCK_DATA_ADDR    MOCK_SECTOR_SIZE
#define MOCK_TOTAL_SLOTS  ((MOCK_FLASH_SIZE - MOCK_SECTOR_SIZE) / MOCK_SLOT_SIZE)
#define MOCK_SLOTS_PER_SECTOR (MOCK_SECTOR_SIZE / MOCK_SLOT_SIZE)

static uint8_t g_flash[MOCK_FLASH_SIZE];
static int g_erase_count;
static int g_write_count;
static int g_read_count;
static bool g_fail_erase;
static bool g_fail_write;
static bool g_fail_read;
static int g_fail_read_after;   /* fail reads after this many, -1 = never */
static bool g_write_lands_then_fails;   /* data goes in, the call says no */
static int g_overprogram;       /* bytes programmed over cells not erased */
#define MOCK_ERASE_LOG 512
static uint32_t g_erase_log[MOCK_ERASE_LOG];

static void mock_flash_reset(void)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_erase_count = 0;
    g_write_count = 0;
    g_read_count = 0;
    g_fail_erase = false;
    g_fail_write = false;
    g_fail_read = false;
    g_fail_read_after = -1;
    g_write_lands_then_fails = false;
    g_overprogram = 0;
}

static int mock_erase(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (g_erase_count < MOCK_ERASE_LOG) g_erase_log[g_erase_count] = addr;
    g_erase_count++;
    if (g_fail_erase) return -1;
    uint32_t end = addr + MOCK_SECTOR_SIZE;
    if (end > MOCK_FLASH_SIZE) end = MOCK_FLASH_SIZE;
    memset(&g_flash[addr], 0xFF, end - addr);
    return 0;
}

/* NOR flash: a program only clears bits, so a cell has to be erased first. */
static int mock_write(void *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    g_write_count++;
    if (g_fail_write) return -1;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    for (uint16_t i = 0; i < len; i++) {
        if (g_flash[addr + i] != 0xFF) g_overprogram++;
        g_flash[addr + i] &= data[i];
    }
    return g_write_lands_then_fails ? -1 : 0;
}

static int mock_read(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    g_read_count++;
    if (g_fail_read) return -1;
    if (g_fail_read_after >= 0 && g_read_count > g_fail_read_after) return -1;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, &g_flash[addr], len);
    return 0;
}

static const flash_ring_io_t g_mock_io = {
    .erase_sector = mock_erase,
    .write        = mock_write,
    .read         = mock_read,
    .ctx          = NULL,
};

static const flash_ring_cfg_t g_mock_cfg = {
    .slot_size       = MOCK_SLOT_SIZE,
    .total_slots     = MOCK_TOTAL_SLOTS,
    .sector_size     = MOCK_SECTOR_SIZE,
    .header_addr     = MOCK_HDR_ADDR,
    .data_start_addr = MOCK_DATA_ADDR,
    .persist_interval = 0,
};

/* ---------- tests ---------- */

TEST(test_init_null)
{
    flash_ring_t ring;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(NULL, &g_mock_io, &g_mock_cfg));
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, NULL, &g_mock_cfg));
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, NULL));
}

TEST(test_init_bad_cfg)
{
    flash_ring_t ring;
    flash_ring_cfg_t bad = g_mock_cfg;
    bad.slot_size = 0;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));
    bad = g_mock_cfg;
    bad.total_slots = 0;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));
}

/*
 * push erases whole sectors, so a slot that straddles a sector boundary
 * loses live frames, and a slot larger than a sector is never erased.
 * Each layout below breaks that, so init must refuse it.
 */
TEST(test_init_rejects_bad_geometry)
{
    flash_ring_t ring;
    mock_flash_reset();

    flash_ring_cfg_t bad = g_mock_cfg;
    bad.slot_size = 100;                /* 4096 % 100 != 0 */
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));

    bad = g_mock_cfg;
    bad.slot_size = 8192;               /* two sectors per slot */
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));

    bad = g_mock_cfg;
    bad.data_start_addr = MOCK_DATA_ADDR + MOCK_SLOT_SIZE; /* mid-sector */
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));
}

/* The limit case stays legal: one slot filling one sector exactly. */
TEST(test_init_accepts_slot_equal_to_sector)
{
    flash_ring_t ring;
    mock_flash_reset();

    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.slot_size = MOCK_SECTOR_SIZE;
    cfg.total_slots = 4;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_mock_io, &cfg));
}

TEST(test_init_fresh_flash)
{
    flash_ring_t ring;
    mock_flash_reset();
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_mock_io, &g_mock_cfg));
    ASSERT_EQ(0u, flash_ring_count(&ring));
    ASSERT_EQ(0u, ring.write_idx);
    ASSERT_EQ(0u, ring.read_idx);
}

TEST(test_init_existing_header)
{
    flash_ring_t ring;
    mock_flash_reset();
    /* First init writes header */
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    /* Push some data */
    uint8_t frame[MOCK_SLOT_SIZE] = {0xAA};
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);

    /* Re-init should recover state */
    flash_ring_t ring2;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring2, &g_mock_io, &g_mock_cfg));
    ASSERT_EQ(2u, flash_ring_count(&ring2));
    ASSERT_EQ(ring.write_idx, ring2.write_idx);
}

TEST(test_push_pop_one)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x42, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(1u, flash_ring_count(&ring));

    uint8_t buf[MOCK_SLOT_SIZE] = {0};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(0x42, buf[0]);
    ASSERT_EQ(0x42, buf[MOCK_SLOT_SIZE - 1]);

    ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
    ASSERT_EQ(0u, flash_ring_count(&ring));
}

TEST(test_pop_empty)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    ASSERT_EQ(FLASH_RING_ERR_EMPTY, flash_ring_pop(&ring));
}

TEST(test_push_wraps_around)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    /* Fill all slots */
    for (uint32_t i = 0; i < MOCK_TOTAL_SLOTS; i++) {
        memset(frame, (uint8_t)(i & 0xFF), sizeof(frame));
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    /* write_idx should wrap to 0 */
    ASSERT_EQ(0u, ring.write_idx);
}

TEST(test_push_null_frame)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_push(&ring, NULL, 10));
}

TEST(test_push_oversized)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    uint8_t frame[MOCK_SLOT_SIZE + 1];
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE + 1));
}

TEST(test_push_io_fail)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    g_fail_write = true;
    uint8_t frame[MOCK_SLOT_SIZE] = {0};
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
}

TEST(test_peek_bad_idx)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    uint8_t buf[10];
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_peek(&ring, MOCK_TOTAL_SLOTS, buf, 10));
}

/* W1A-11: len past the slot read into the next slot, or past the region. */
TEST(test_peek_refuses_len_past_the_slot)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x21};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));

    uint8_t buf[MOCK_SLOT_SIZE + 1];
    ASSERT_EQ(FLASH_RING_ERR_PARAM,
              flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE + 1));
    ASSERT_EQ(FLASH_RING_OK,
              flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE));
}

/* W1A-11: an empty ring has no frame to show, whatever the slot holds. */
TEST(test_peek_empty_ring)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t buf[4];
    ASSERT_EQ(FLASH_RING_ERR_EMPTY, flash_ring_peek(&ring, 0, buf, 4));

    uint8_t frame[4] = {1, 2, 3, 4};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, 4));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
    ASSERT_EQ(FLASH_RING_ERR_EMPTY, flash_ring_peek(&ring, 0, buf, 4));
}

/* W1A-11: only read_idx up to write_idx - 1 holds live frames. */
TEST(test_peek_refuses_slots_outside_the_live_range)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x23};
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));

    uint8_t buf[4];
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_peek(&ring, 0, buf, 4)); /* popped */
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, 1, buf, 4));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, 2, buf, 4));
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_peek(&ring, 3, buf, 4)); /* write_idx */
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_peek(&ring, 100, buf, 4));
}

/* W1A-11: the same once the live range wraps past the end of the ring. */
TEST(test_peek_live_range_across_the_wrap)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x24};
    for (uint32_t i = 0; i < MOCK_TOTAL_SLOTS + 5u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(5u, ring.write_idx);
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, ring.read_idx);

    uint8_t buf[4];
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, 4, buf, 4));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, MOCK_TOTAL_SLOTS - 1u, buf, 4));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, ring.read_idx, buf, 4));
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_peek(&ring, 5, buf, 4));
    ASSERT_EQ(FLASH_RING_ERR_PARAM,
              flash_ring_peek(&ring, ring.read_idx - 1u, buf, 4));
}

TEST(test_discard_all)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0xBB};
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    ASSERT_EQ(2u, flash_ring_count(&ring));

    ASSERT_EQ(FLASH_RING_OK, flash_ring_discard_all(&ring));
    ASSERT_EQ(0u, flash_ring_count(&ring));
}

TEST(test_flush_header_explicit)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);
    uint8_t frame[MOCK_SLOT_SIZE] = {0xCC};
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    int before = g_erase_count;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));
    ASSERT_TRUE(g_erase_count > before);
}

TEST(test_persist_interval)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 4;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0xDD};
    int erase_after_init = g_erase_count;

    /* Pushes 1-3: no header flush */
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    /* Only data sector erases, header not flushed */
    int erase_before_4th = g_erase_count;
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    /* 4th push triggers header flush (erase header sector) */
    ASSERT_TRUE(g_erase_count > erase_before_4th);
    (void)erase_after_init;
}

TEST(test_count_null)
{
    ASSERT_EQ(0u, flash_ring_count(NULL));
}

TEST(test_multiple_push_pop_sequence)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    for (int i = 0; i < 10; i++) {
        memset(frame, (uint8_t)i, sizeof(frame));
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(10u, flash_ring_count(&ring));

    /* Pop 5, verify remaining */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
    }
    ASSERT_EQ(5u, flash_ring_count(&ring));

    /* Peek first remaining should have pattern 5 */
    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(5, buf[0]);
}

TEST(test_init_read_fail)
{
    flash_ring_t ring;
    mock_flash_reset();
    g_fail_read = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_init(&ring, &g_mock_io, &g_mock_cfg));
}

/* ---------- new tests ---------- */

/* io.erase_sector=NULL must return ERR_PARAM */
TEST(test_init_null_erase)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_io_t bad = g_mock_io;
    bad.erase_sector = NULL;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &bad, &g_mock_cfg));
}

/* io.write=NULL must return ERR_PARAM */
TEST(test_init_null_write)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_io_t bad = g_mock_io;
    bad.write = NULL;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &bad, &g_mock_cfg));
}

/* io.read=NULL must return ERR_PARAM */
TEST(test_init_null_read)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_io_t bad = g_mock_io;
    bad.read = NULL;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &bad, &g_mock_cfg));
}

/*
 * push erases the next sector before it programs the last slot of the current
 * one. A failed erase must return ERR_IO, write nothing and leave write_idx
 * where it is, so that a retry repeats the erase.
 */
TEST(test_push_erase_fail)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x11};
    for (uint32_t i = 0; i < MOCK_SLOTS_PER_SECTOR - 1u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }

    g_fail_erase = true;
    uint32_t before = ring.write_idx;
    int writes = g_write_count;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(before, ring.write_idx);
    ASSERT_EQ(writes, g_write_count);

    g_fail_erase = false;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(before + 1u, ring.write_idx);
    ASSERT_EQ(0, g_overprogram);
}

/* A failed erase may have damaged the sector: the reader must not stay in it. */
TEST(test_failed_erase_still_moves_the_reader_on)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x31};
    for (uint32_t i = 0; i < MOCK_TOTAL_SLOTS - 1u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(0u, ring.read_idx);

    g_fail_erase = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, ring.read_idx);
}

/*
 * Fresh flash (no magic) causes init to call flush_header.
 * If erase fails during that flush, init must return ERR_IO.
 */
TEST(test_init_fresh_flash_erase_fail)
{
    flash_ring_t ring;
    mock_flash_reset();          /* all 0xFF → no valid magic */
    g_fail_erase = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_init(&ring, &g_mock_io, &g_mock_cfg));
}

/*
 * persist_interval=4: flush fires every 4th write/pop (since_flush counts
 * both push and pop operations). After init (fresh), since_flush=0.
 * Push 4 frames — 4th push triggers flush (extra erase).
 * Pop 3 frames — since_flush resets to 0 on the 4th push, then increments
 * for each pop. Pop 3 more won't cross the threshold.
 * Pop a 4th time — since_flush reaches 4 → flush triggered (erase count rises).
 */
TEST(test_pop_persist_interval)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 4;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0xEE};

    /* Push 4 frames; the 4th flush resets since_flush to 0 */
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    int erases_before_4th_push = g_erase_count;
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE); /* triggers flush */
    ASSERT_TRUE(g_erase_count > erases_before_4th_push);

    /* since_flush is now 0; pop 3 — no flush yet */
    int erases_before_pops = g_erase_count;
    flash_ring_pop(&ring);
    flash_ring_pop(&ring);
    flash_ring_pop(&ring);
    ASSERT_EQ(erases_before_pops, g_erase_count);

    /* 4th pop reaches threshold → flush */
    flash_ring_pop(&ring);
    ASSERT_TRUE(g_erase_count > erases_before_pops);
}

/*
 * Push (total_slots + 5) frames without popping. The writer has erased sector
 * 0 ahead of itself, so the oldest frame left is the first one of sector 1.
 */
TEST(test_count_after_wrap)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    uint32_t total = MOCK_TOTAL_SLOTS;
    for (uint32_t i = 0; i < total + 5u; i++) {
        memset(frame, (uint8_t)(i & 0xFF), sizeof(frame));
        flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    }

    ASSERT_EQ(5u, ring.write_idx);
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, ring.read_idx);
    ASSERT_EQ(total - MOCK_SLOTS_PER_SECTOR + 5u, flash_ring_count(&ring));

    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK,
              flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, buf[0]);
    ASSERT_EQ(0, g_overprogram);
}

/*
 * W1A-2: a ring that has gone all the way round must not read as empty.
 * Before the fix the push that filled the last slot left write_idx on
 * read_idx, so count was 0 and pop returned EMPTY with every frame in flash.
 */
TEST(test_full_ring_does_not_read_as_empty)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    for (uint32_t i = 0; i < MOCK_TOTAL_SLOTS; i++) {
        memset(frame, (uint8_t)i, sizeof(frame));
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
        ASSERT_TRUE(flash_ring_count(&ring) > 0u);
    }

    uint32_t left = MOCK_TOTAL_SLOTS - MOCK_SLOTS_PER_SECTOR;
    ASSERT_EQ(left, flash_ring_count(&ring));

    /* Every frame left is intact, oldest first, up to the newest. */
    uint8_t buf[MOCK_SLOT_SIZE];
    uint32_t want = MOCK_SLOTS_PER_SECTOR;
    while (flash_ring_count(&ring) > 0u) {
        ASSERT_EQ(FLASH_RING_OK,
                  flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE));
        ASSERT_EQ((uint8_t)want, buf[0]);
        ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
        want++;
    }
    ASSERT_EQ(MOCK_TOTAL_SLOTS, want);
}

/*
 * W1A-2: a reader part way through the sector the writer is about to erase.
 * Its frames are gone, so read_idx has to move past them, not stay on 0xFF.
 */
TEST(test_read_idx_leaves_a_sector_being_erased)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    for (uint32_t i = 0; i < MOCK_SLOTS_PER_SECTOR + 3u; i++) {
        memset(frame, 0x11, sizeof(frame));
        flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);
    }
    /* Reader lands in sector 1, three slots in, one frame short of write_idx */
    for (uint32_t i = 0; i < MOCK_SLOTS_PER_SECTOR + 2u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
    }
    ASSERT_EQ(1u, flash_ring_count(&ring));

    /* Write up to the last slot of the sector before sector 1, a lap later */
    while (ring.write_idx != 0u) {
        memset(frame, 0x22, sizeof(frame));
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    for (uint32_t i = 0; i < MOCK_SLOTS_PER_SECTOR; i++) {
        memset(frame, 0x33, sizeof(frame));
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }

    ASSERT_EQ(2u * MOCK_SLOTS_PER_SECTOR, ring.read_idx);
    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK,
              flash_ring_peek(&ring, ring.read_idx, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(0x22, buf[0]);
    ASSERT_EQ(0, g_overprogram);
}

/*
 * Q24-3: the data erases of a lap, by address. Each one has to land exactly
 * once, on the sector after the one being finished, just before its last slot.
 */
TEST(test_erases_each_sector_once_per_lap)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x5A};
    uint32_t sectors = MOCK_TOTAL_SLOTS / MOCK_SLOTS_PER_SECTOR;
    for (uint32_t lap = 0; lap < 2u; lap++) {
        for (uint32_t s = 0; s < sectors; s++) {
            for (uint32_t k = 0; k < MOCK_SLOTS_PER_SECTOR; k++) {
                int before = g_erase_count;
                ASSERT_EQ(FLASH_RING_OK,
                          flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
                int data_erases = 0;
                uint32_t addr = 0;
                for (int e = before; e < g_erase_count; e++) {
                    if (g_erase_log[e] != MOCK_HDR_ADDR) {
                        data_erases++;
                        addr = g_erase_log[e];
                    }
                }
                if (k + 1u < MOCK_SLOTS_PER_SECTOR) {
                    ASSERT_EQ(0, data_erases);
                } else {
                    uint32_t next = (s + 1u) % sectors;
                    ASSERT_EQ(1, data_erases);
                    ASSERT_EQ(MOCK_DATA_ADDR + next * MOCK_SECTOR_SIZE, addr);
                }
            }
        }
    }
    ASSERT_EQ(0, g_overprogram);
}

/* A single-sector ring would have nothing to erase ahead of itself. */
TEST(test_init_refuses_a_single_sector_ring)
{
    flash_ring_t ring;
    mock_flash_reset();

    flash_ring_cfg_t bad = g_mock_cfg;
    bad.total_slots = MOCK_SLOTS_PER_SECTOR;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));

    bad.total_slots = MOCK_SLOTS_PER_SECTOR - 1u;
    ASSERT_EQ(FLASH_RING_ERR_PARAM, flash_ring_init(&ring, &g_mock_io, &bad));

    /* One slot into a second sector is the smallest ring that works. */
    flash_ring_cfg_t ok = g_mock_cfg;
    ok.total_slots = MOCK_SLOTS_PER_SECTOR + 1u;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_mock_io, &ok));
}

/* A ring on flash holding old data: push must not program over it. */
TEST(test_fresh_ring_erases_its_first_sector)
{
    flash_ring_t ring;
    mock_flash_reset();
    memset(&g_flash[MOCK_DATA_ADDR], 0xA5, MOCK_SECTOR_SIZE * 2u);

    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_mock_io, &g_mock_cfg));
    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x3C, sizeof(frame));
    for (uint32_t i = 0; i < MOCK_SLOTS_PER_SECTOR + 2u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(0, g_overprogram);
    ASSERT_EQ(0x3C, g_flash[MOCK_DATA_ADDR + MOCK_SECTOR_SIZE + MOCK_SLOT_SIZE]);
}

/*
 * W1A-4: the header on flash is up to persist_interval pushes old. The README
 * example uses 16: 25 pushes leave a header saying 16. Before the fix the next
 * push programmed over slot 16 and returned OK.
 */
TEST(test_reset_finds_frames_newer_than_the_header)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 16;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    for (uint32_t i = 0; i < 25u; i++) {
        memset(frame, (uint8_t)i, sizeof(frame));
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(25u, after.write_idx);
    ASSERT_EQ(25u, flash_ring_count(&after));
    ASSERT_EQ(25u, after.frame_count);

    memset(frame, 0x2A, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(0, g_overprogram);

    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&after, 25u, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(0x2A, buf[0]);
}

/*
 * The header can be stale right across a sector boundary in a ring that has
 * wrapped, where the slots ahead hold old frames. Erasing ahead is what keeps
 * write_idx on a blank slot there.
 */
TEST(test_reset_on_a_sector_boundary_after_a_lap)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x44, sizeof(frame));
    for (uint32_t i = 0; i < MOCK_TOTAL_SLOTS + MOCK_SLOTS_PER_SECTOR; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, ring.write_idx);
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(ring.write_idx, after.write_idx);
    ASSERT_EQ(ring.read_idx, after.read_idx);
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(0, g_overprogram);
}

/* The walk replays the same rule as push, so read_idx leaves a sector the
 * writer erased after the last header flush. */
TEST(test_reset_moves_read_idx_out_of_an_erased_sector)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x66, sizeof(frame));
    uint32_t to_end = MOCK_TOTAL_SLOTS - 3u;
    for (uint32_t i = 0; i < to_end; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring)); /* read_idx 0 */
    ASSERT_EQ(0u, ring.read_idx);

    for (uint32_t i = 0; i < 5u; i++) {     /* crosses into sector 0 again */
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, ring.read_idx);

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(ring.write_idx, after.write_idx);
    ASSERT_EQ(ring.read_idx, after.read_idx);
}

/*
 * A cut between push's erase and its program: the next sector is blank, the
 * last slot is not written and the stored read_idx still points into the
 * blank sector. Nothing was written, so only a check of read_idx can see it.
 */
TEST(test_reset_after_a_cut_between_erase_and_program)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x77, sizeof(frame));
    for (uint32_t i = 0; i < MOCK_TOTAL_SLOTS - 1u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));
    ASSERT_EQ(0u, ring.read_idx);

    /* The push of the last slot gets as far as its erase and no further. */
    g_fail_write = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    g_fail_write = false;

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(MOCK_TOTAL_SLOTS - 1u, after.write_idx);
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, after.read_idx);

    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK,
              flash_ring_peek(&after, after.read_idx, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(0x77, buf[0]);
}

/*
 * An interval longer than the ring: the writer comes back round and erases the
 * sector the stored write_idx points into, so the header has to be rewritten
 * before that, whatever the interval says. Reset after every push of 3 laps.
 */
TEST(test_header_is_flushed_before_the_writer_laps_it)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.total_slots = 2u * MOCK_SLOTS_PER_SECTOR + 3u;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x4D, sizeof(frame));
    for (uint32_t i = 0; i < 3u * cfg.total_slots; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));

        flash_ring_t after;
        ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
        ASSERT_EQ(ring.write_idx, after.write_idx);
    }
    ASSERT_EQ(0, g_overprogram);
}

/*
 * Resets more often than persist_interval: no push ever reaches the interval,
 * so a counter that starts again at each init never writes the header. The
 * walk has to count the pushes it finds, or the writer laps the stored position
 * and the ring loses its place.
 */
TEST(test_resets_more_often_than_the_interval)
{
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.total_slots = 3u * MOCK_SLOTS_PER_SECTOR;
    cfg.persist_interval = 12;

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x3E, sizeof(frame));
    uint32_t pushed = 0;
    for (uint32_t boot = 0; boot < 60u; boot++) {
        flash_ring_t ring;
        ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_mock_io, &cfg));
        ASSERT_EQ(pushed % cfg.total_slots, ring.write_idx);
        for (int i = 0; i < 8; i++) {
            ASSERT_EQ(FLASH_RING_OK,
                      flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
            pushed++;
        }
    }
    ASSERT_EQ(0, g_overprogram);
}

/*
 * OK from push means the frame is stored. A header write that fails afterwards
 * only leaves the header older, and the next operation tries again. Reporting
 * it as a failed push makes a caller store the same frame twice.
 */
TEST(test_push_is_ok_when_only_the_header_write_fails)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 3;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x71};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));

    g_fail_erase = true;                /* the third push wants the header */
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(3u, flash_ring_count(&ring));

    /* Still due, so the very next operation tries again */
    int before = g_erase_count;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
    ASSERT_EQ(before + 1, g_erase_count);
    g_fail_erase = false;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(&ring));
    ASSERT_EQ(0, ring.since_flush);
}

/* The counter must not wrap past the interval while the header keeps failing. */
TEST(test_flush_stays_due_at_the_largest_interval)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    cfg.total_slots = 0x20000u;         /* the limit from the ring size is higher */
    /* init only reads and erases near the start, so the mock is big enough */
    flash_ring_init(&ring, &g_mock_io, &cfg);
    ring.since_flush = 0xFFFE;

    uint8_t frame[MOCK_SLOT_SIZE] = {0x72};
    g_fail_erase = true;
    for (int i = 0; i < 3; i++) {
        int before = g_erase_count;
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
        if (i > 0) {
            ASSERT_EQ(before + 1, g_erase_count);
        }
    }
    ASSERT_EQ(0xFFFF, ring.since_flush);
}

/*
 * A program that reports failure after the data went in: the cells are no
 * longer blank, so the next push must not program them again.
 */
TEST(test_failed_program_that_landed_is_left_behind)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x55, sizeof(frame));
    g_write_lands_then_fails = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    g_write_lands_then_fails = false;
    ASSERT_EQ(1u, ring.write_idx);

    memset(frame, 0xAA, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(0, g_overprogram);

    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, 1, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(0xAA, buf[0]);
}

/* A failed program that changed nothing keeps the slot, as a retry can use it. */
TEST(test_failed_program_that_did_nothing_keeps_the_slot)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x56};
    g_fail_write = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    g_fail_write = false;
    ASSERT_EQ(0u, ring.write_idx);
    ASSERT_EQ(0u, flash_ring_count(&ring));
}

/*
 * The erase-on-entry code cleared a sector on entering it and left read_idx
 * alone, so it can leave read_idx ahead of write_idx in the blank tail.
 */
TEST(test_old_ring_with_read_idx_in_the_blank_tail)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    memset(&g_flash[MOCK_DATA_ADDR], 0x5C, MOCK_TOTAL_SLOTS * MOCK_SLOT_SIZE);
    uint32_t w = MOCK_SLOTS_PER_SECTOR + 5u;
    memset(&g_flash[MOCK_DATA_ADDR + w * MOCK_SLOT_SIZE], 0xFF,
           (2u * MOCK_SLOTS_PER_SECTOR - w) * MOCK_SLOT_SIZE);
    ring.write_idx = w;
    ring.read_idx = w + 1u;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(w, after.write_idx);
    ASSERT_EQ(2u * MOCK_SLOTS_PER_SECTOR, after.read_idx);

    uint8_t buf[MOCK_SLOT_SIZE];
    ASSERT_EQ(FLASH_RING_OK,
              flash_ring_peek(&after, after.read_idx, buf, MOCK_SLOT_SIZE));
    ASSERT_EQ(0x5C, buf[0]);
}

/*
 * With no blank slot and a stored write_idx part way through a sector, the
 * rest of that sector was written after the header, so those are the newest
 * frames: the sector to give up is the next one. The header must then say so,
 * or the next reset walks from a position that no longer exists.
 */
TEST(test_old_full_ring_keeps_the_newest_frames)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    memset(&g_flash[MOCK_DATA_ADDR], 0x5C, MOCK_TOTAL_SLOTS * MOCK_SLOT_SIZE);
    ring.write_idx = MOCK_TOTAL_SLOTS - 1u;
    ring.read_idx = 0;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(0u, after.write_idx);
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, after.read_idx);
    ASSERT_EQ(0x5C, g_flash[MOCK_DATA_ADDR + (MOCK_TOTAL_SLOTS - 1u) * MOCK_SLOT_SIZE]);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x01};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, frame, MOCK_SLOT_SIZE));

    flash_ring_t again;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&again, &g_mock_io, &cfg));
    ASSERT_EQ(1u, again.write_idx);
    ASSERT_EQ(MOCK_SLOTS_PER_SECTOR, again.read_idx);
    ASSERT_EQ(0, g_overprogram);
}

/* An all-0xFF frame looks blank on its own. Frames after it must still count. */
TEST(test_reset_walks_past_an_all_ff_frame)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x10, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    memset(frame, 0xFF, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    memset(frame, 0x12, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(3u, after.write_idx);
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(0, g_overprogram);
}

/*
 * The old code erased a sector on entering it, so a ring it wrapped with
 * write_idx on a sector boundary has no blank slot anywhere. Init must make
 * one rather than program over old frames.
 */
TEST(test_reset_of_a_ring_with_no_blank_slot)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    memset(&g_flash[MOCK_DATA_ADDR], 0x5C, MOCK_TOTAL_SLOTS * MOCK_SLOT_SIZE);
    ring.write_idx = 2u * MOCK_SLOTS_PER_SECTOR;
    ring.read_idx = 2u * MOCK_SLOTS_PER_SECTOR + 3u;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(2u * MOCK_SLOTS_PER_SECTOR, after.write_idx);
    ASSERT_EQ(3u * MOCK_SLOTS_PER_SECTOR, after.read_idx);
    ASSERT_EQ(ring.frame_count, after.frame_count);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x01, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(0, g_overprogram);
}

/* The same with an empty ring and write_idx part way through a sector: the
 * next sector is the one to erase, and the ring stays empty. */
TEST(test_reset_of_a_ring_with_no_blank_slot_mid_sector)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    memset(&g_flash[MOCK_DATA_ADDR], 0x5C, MOCK_TOTAL_SLOTS * MOCK_SLOT_SIZE);
    ring.write_idx = 2u * MOCK_SLOTS_PER_SECTOR + 5u;
    ring.read_idx = ring.write_idx;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_flush_header(&ring));

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(3u * MOCK_SLOTS_PER_SECTOR, after.write_idx);
    ASSERT_EQ(0u, flash_ring_count(&after));

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x02, sizeof(frame));
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, frame, MOCK_SLOT_SIZE));
    ASSERT_EQ(0, g_overprogram);
}

/* A partial last sector: what lies past the ring's end is not ring data. */
TEST(test_blank_check_stops_at_the_end_of_the_ring)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.total_slots = 2u * MOCK_SLOTS_PER_SECTOR + 3u;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE];
    memset(frame, 0x6E, sizeof(frame));
    for (uint32_t i = 0; i < 2u * MOCK_SLOTS_PER_SECTOR + 1u; i++) {
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));
    }
    g_flash[MOCK_DATA_ADDR + cfg.total_slots * MOCK_SLOT_SIZE] = 0x00;

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_mock_io, &cfg));
    ASSERT_EQ(ring.write_idx, after.write_idx);
}

TEST(test_reset_read_error_during_the_walk)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_cfg_t cfg = g_mock_cfg;
    cfg.persist_interval = 0xFFFF;
    flash_ring_init(&ring, &g_mock_io, &cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x09};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, MOCK_SLOT_SIZE));

    g_read_count = 0;
    g_fail_read_after = 1;              /* the header read succeeds, no more */
    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_init(&after, &g_mock_io, &cfg));
}

/*
 * discard_all calls flush_header internally. If erase fails during that
 * flush, it must propagate ERR_IO.
 */
TEST(test_discard_io_fail)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[MOCK_SLOT_SIZE] = {0x77};
    flash_ring_push(&ring, frame, MOCK_SLOT_SIZE);

    g_fail_erase = true;
    ASSERT_EQ(FLASH_RING_ERR_IO, flash_ring_discard_all(&ring));
}

/*
 * Minimum-length frame (1 byte). Peek must return that exact byte.
 */
TEST(test_push_min_len)
{
    flash_ring_t ring;
    mock_flash_reset();
    flash_ring_init(&ring, &g_mock_io, &g_mock_cfg);

    uint8_t frame[1] = {0xA7};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, frame, 1));
    ASSERT_EQ(1u, flash_ring_count(&ring));

    uint8_t buf[1] = {0x00};
    ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(&ring, ring.read_idx, buf, 1));
    ASSERT_EQ(0xA7, buf[0]);
}

TEST_MAIN("flash_ring")
    RUN(test_init_null);
    RUN(test_init_bad_cfg);
    RUN(test_init_rejects_bad_geometry);
    RUN(test_init_accepts_slot_equal_to_sector);
    RUN(test_init_fresh_flash);
    RUN(test_init_existing_header);
    RUN(test_push_pop_one);
    RUN(test_pop_empty);
    RUN(test_push_wraps_around);
    RUN(test_push_null_frame);
    RUN(test_push_oversized);
    RUN(test_push_io_fail);
    RUN(test_peek_bad_idx);
    RUN(test_peek_refuses_len_past_the_slot);
    RUN(test_peek_empty_ring);
    RUN(test_peek_refuses_slots_outside_the_live_range);
    RUN(test_peek_live_range_across_the_wrap);
    RUN(test_discard_all);
    RUN(test_flush_header_explicit);
    RUN(test_persist_interval);
    RUN(test_count_null);
    RUN(test_multiple_push_pop_sequence);
    RUN(test_init_read_fail);
    RUN(test_init_null_erase);
    RUN(test_init_null_write);
    RUN(test_init_null_read);
    RUN(test_push_erase_fail);
    RUN(test_failed_erase_still_moves_the_reader_on);
    RUN(test_init_fresh_flash_erase_fail);
    RUN(test_pop_persist_interval);
    RUN(test_count_after_wrap);
    RUN(test_full_ring_does_not_read_as_empty);
    RUN(test_read_idx_leaves_a_sector_being_erased);
    RUN(test_erases_each_sector_once_per_lap);
    RUN(test_init_refuses_a_single_sector_ring);
    RUN(test_fresh_ring_erases_its_first_sector);
    RUN(test_reset_finds_frames_newer_than_the_header);
    RUN(test_reset_on_a_sector_boundary_after_a_lap);
    RUN(test_reset_moves_read_idx_out_of_an_erased_sector);
    RUN(test_reset_after_a_cut_between_erase_and_program);
    RUN(test_header_is_flushed_before_the_writer_laps_it);
    RUN(test_resets_more_often_than_the_interval);
    RUN(test_push_is_ok_when_only_the_header_write_fails);
    RUN(test_flush_stays_due_at_the_largest_interval);
    RUN(test_failed_program_that_landed_is_left_behind);
    RUN(test_failed_program_that_did_nothing_keeps_the_slot);
    RUN(test_old_ring_with_read_idx_in_the_blank_tail);
    RUN(test_old_full_ring_keeps_the_newest_frames);
    RUN(test_reset_walks_past_an_all_ff_frame);
    RUN(test_reset_of_a_ring_with_no_blank_slot);
    RUN(test_reset_of_a_ring_with_no_blank_slot_mid_sector);
    RUN(test_blank_check_stops_at_the_end_of_the_ring);
    RUN(test_reset_read_error_during_the_walk);
    RUN(test_discard_io_fail);
    RUN(test_push_min_len);
TEST_MAIN_END()
