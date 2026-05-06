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

static uint8_t g_flash[MOCK_FLASH_SIZE];
static int g_erase_count;
static int g_write_count;
static int g_read_count;
static bool g_fail_erase;
static bool g_fail_write;
static bool g_fail_read;

static void mock_flash_reset(void)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_erase_count = 0;
    g_write_count = 0;
    g_read_count = 0;
    g_fail_erase = false;
    g_fail_write = false;
    g_fail_read = false;
}

static int mock_erase(void *ctx, uint32_t addr)
{
    (void)ctx;
    g_erase_count++;
    if (g_fail_erase) return -1;
    uint32_t end = addr + MOCK_SECTOR_SIZE;
    if (end > MOCK_FLASH_SIZE) end = MOCK_FLASH_SIZE;
    memset(&g_flash[addr], 0xFF, end - addr);
    return 0;
}

static int mock_write(void *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    g_write_count++;
    if (g_fail_write) return -1;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    memcpy(&g_flash[addr], data, len);
    return 0;
}

static int mock_read(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    g_read_count++;
    if (g_fail_read) return -1;
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

TEST_MAIN("flash_ring")
    RUN(test_init_null);
    RUN(test_init_bad_cfg);
    RUN(test_init_fresh_flash);
    RUN(test_init_existing_header);
    RUN(test_push_pop_one);
    RUN(test_pop_empty);
    RUN(test_push_wraps_around);
    RUN(test_push_null_frame);
    RUN(test_push_oversized);
    RUN(test_push_io_fail);
    RUN(test_peek_bad_idx);
    RUN(test_discard_all);
    RUN(test_flush_header_explicit);
    RUN(test_persist_interval);
    RUN(test_count_null);
    RUN(test_multiple_push_pop_sequence);
    RUN(test_init_read_fail);
TEST_MAIN_END()
