// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <stdlib.h>
#include "w25qxx.h"

/* ---------- mock SPI flash (64KB simulated) ---------- */

#define SR1_BUSY   0x01
#define MOCK_SIZE  65536U
static uint8_t mock_mem[MOCK_SIZE];
static int     mock_cs;
static int     mock_phase;  /* 0=idle, 1=cmd, 2=addr, 3=data */
static uint8_t mock_cmd;
static uint32_t mock_addr;
static int     mock_addr_bytes;
static uint8_t mock_status;
static int     mock_fail_next;
static uint32_t mock_jedec;
static int     mock_stuck_busy;

/* A program or erase cycle: it starts when /CS rises and lasts this many status
 * reads, the only clock the mock has. */
#define MOCK_BUSY_READS 3u
static uint8_t  mock_page_buf[256];
static uint32_t mock_page_sent;
static uint8_t  mock_running;
static uint32_t mock_running_addr;
static uint32_t mock_busy_left;

static void mock_reset(void)
{
    memset(mock_mem, 0xFF, MOCK_SIZE);
    mock_cs = 1;
    mock_phase = 0;
    mock_cmd = 0;
    mock_addr = 0;
    mock_addr_bytes = 0;
    mock_status = 0;
    mock_fail_next = 0;
    mock_jedec = 0xEF4017; /* W25Q64 */
    mock_stuck_busy = 0;
    mock_page_sent = 0;
    mock_running = 0;
    mock_running_addr = 0;
    mock_busy_left = 0;
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs = 0;
    mock_phase = 1;
    mock_addr_bytes = 0;
    mock_page_sent = 0;
}

static void mock_start_cycle(void)
{
    mock_running = mock_cmd;
    mock_running_addr = mock_addr;
    mock_busy_left = MOCK_BUSY_READS;
    mock_status |= SR1_BUSY;
}

/* The array changes, and BUSY and WEL clear (manual p.36) */
static void mock_end_cycle(void)
{
    if (mock_running == 0x02) {
        uint32_t page = mock_running_addr & ~0xFFU;
        for (uint32_t i = 0; i < 256u && page + i < MOCK_SIZE; i++)
            mock_mem[page + i] &= mock_page_buf[i]; /* NOR flash: can only clear bits */
    } else if (mock_running == 0x20) {
        uint32_t base = mock_running_addr & ~0xFFFU;
        uint32_t end = base + 4096;
        if (end > MOCK_SIZE) end = MOCK_SIZE;
        for (uint32_t i = base; i < end; i++) mock_mem[i] = 0xFF;
    } else {
        memset(mock_mem, 0xFF, MOCK_SIZE);
    }
    mock_running = 0;
    mock_status = (uint8_t)(mock_status & ~0x03);
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    if (!(mock_status & SR1_BUSY) && (mock_status & 0x02)) {
        bool program = mock_cmd == 0x02 && mock_phase == 3 && mock_page_sent != 0;
        bool sector  = mock_cmd == 0x20 && mock_addr_bytes == 3;
        bool chip    = mock_cmd == 0xC7;
        if (program || sector || chip)
            mock_start_cycle();
    }
    mock_cs = 1;
    mock_phase = 0;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_fail_next) { mock_fail_next = 0; return 0xFF; }

    if (mock_phase == 1) {
        /* p.22: "all instructions except for Read Status Register will be
         * ignored" until the cycle has finished */
        if ((mock_status & SR1_BUSY) && tx != 0x05) {
            mock_phase = 0;
            return 0xFF;
        }
        mock_cmd = tx;
        mock_addr = 0;
        mock_addr_bytes = 0;

        if (tx == 0x06) { mock_status |= 0x02; return 0xFF; } /* WREN → set WEL */
        if (tx == 0x9F) { mock_phase = 3; return 0xFF; }       /* JEDEC ID */
        if (tx == 0x05) { mock_phase = 3; return 0xFF; }       /* Read SR1 */
        if (tx == 0xC7) return 0xFF;                            /* Chip erase */
        if (tx == 0x02) memset(mock_page_buf, 0xFF, sizeof(mock_page_buf));
        mock_phase = 2;
        return 0xFF;
    }

    if (mock_phase == 2) {
        mock_addr = (mock_addr << 8) | tx;
        mock_addr_bytes++;
        if (mock_addr_bytes >= 3) mock_phase = 3;
        return 0xFF;
    }

    if (mock_phase == 3) {
        if (mock_cmd == 0x9F) {
            static int id_byte = 0;
            uint8_t r = (mock_jedec >> (16 - 8 * id_byte)) & 0xFF;
            id_byte = (id_byte + 1) % 3;
            return r;
        }
        if (mock_cmd == 0x05) {
            uint8_t r = (uint8_t)(mock_status | (mock_stuck_busy ? SR1_BUSY : 0));
            if ((mock_status & SR1_BUSY) && --mock_busy_left == 0)
                mock_end_cycle();
            return r;
        }
        if (mock_cmd == 0x03) {
            uint8_t r = (mock_addr < MOCK_SIZE) ? mock_mem[mock_addr] : 0xFF;
            mock_addr++;
            return r;
        }
        if (mock_cmd == 0x02) {
            /* kept until /CS rises, and wraps inside the page (manual p.36) */
            mock_page_buf[(mock_addr + mock_page_sent) & 0xFFu] = tx;
            mock_page_sent++;
            return 0xFF;
        }
    }
    return 0xFF;
}

static const w25qxx_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- test helpers ---------- */

/* ---------- tests ---------- */

TEST(test_init)
{
    mock_reset();
    uint32_t id = 0;
    w25qxx_err_t e = w25qxx_init(&mock_spi, &id);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(id, 0xEF4017);
}

TEST(test_init_bad_id)
{
    mock_reset();
    mock_jedec = 0xFF0000;
    uint32_t id = 0;
    w25qxx_err_t e = w25qxx_init(&mock_spi, &id);
    ASSERT_EQ(e, W25QXX_ERR_JEDEC_ID);
}

TEST(test_read)
{
    mock_reset();
    mock_mem[0x100] = 0xAB;
    mock_mem[0x101] = 0xCD;
    uint8_t buf[2] = {0};
    w25qxx_err_t e = w25qxx_read(&mock_spi, 0x100, buf, 2);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(buf[0], 0xAB);
    ASSERT_EQ(buf[1], 0xCD);
}

TEST(test_page_program)
{
    mock_reset();
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0x200, data, 4);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(mock_mem[0x200], 0x11);
    ASSERT_EQ(mock_mem[0x203], 0x44);
    ASSERT_EQ(mock_mem[0x204], 0xFF);
}

TEST(test_page_program_cross_boundary)
{
    mock_reset();
    uint8_t data[2] = {0xAA, 0xBB};
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0xFF, data, 2);
    ASSERT_EQ(e, W25QXX_ERR_ALIGN);
}

TEST(test_sector_erase)
{
    mock_reset();
    mock_mem[0x1000] = 0x42;
    mock_mem[0x1FFF] = 0x42;
    mock_mem[0x2000] = 0x42;
    w25qxx_err_t e = w25qxx_sector_erase(&mock_spi, 0x1000);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(mock_mem[0x1000], 0xFF);
    ASSERT_EQ(mock_mem[0x1FFF], 0xFF);
    ASSERT_EQ(mock_mem[0x2000], 0x42);
}

TEST(test_chip_erase)
{
    mock_reset();
    mock_mem[0] = 0x42;
    mock_mem[MOCK_SIZE - 1] = 0x42;
    w25qxx_err_t e = w25qxx_chip_erase(&mock_spi);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(mock_mem[0], 0xFF);
    ASSERT_EQ(mock_mem[MOCK_SIZE - 1], 0xFF);
}

TEST(test_read_write_cycle)
{
    mock_reset();
    w25qxx_sector_erase(&mock_spi, 0);

    uint8_t wr[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    w25qxx_page_program(&mock_spi, 0, wr, 8);

    uint8_t rd[8] = {0};
    w25qxx_read(&mock_spi, 0, rd, 8);
    ASSERT_TRUE(memcmp(wr, rd, 8) == 0);
}

TEST(test_read_status)
{
    mock_reset();
    uint8_t sr = 0xFF;
    w25qxx_err_t e = w25qxx_read_status(&mock_spi, &sr);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(sr & 0x01, 0);
}

TEST(test_init_null_jedec_out)
{
    mock_reset();
    w25qxx_err_t e = w25qxx_init(&mock_spi, NULL);
    ASSERT_EQ(e, W25QXX_OK);
}

TEST(test_page_program_full_page)
{
    mock_reset();
    uint8_t data[256];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0x100, data, 256);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(mock_mem[0x100], 0x00);
    ASSERT_EQ(mock_mem[0x1FF], 0xFF);
    ASSERT_EQ(mock_mem[0x200], 0xFF);
}

TEST(test_nor_program_semantics)
{
    mock_reset();
    /* Write 0x0F first */
    uint8_t d1 = 0x0F;
    w25qxx_page_program(&mock_spi, 0x300, &d1, 1);
    ASSERT_EQ(mock_mem[0x300], 0x0F);

    /* Write 0xF0 on top — NOR can only clear bits, not set them */
    uint8_t d2 = 0xF0;
    w25qxx_page_program(&mock_spi, 0x300, &d2, 1);
    ASSERT_EQ(mock_mem[0x300], 0x00);

    /* Erase restores 0xFF */
    w25qxx_sector_erase(&mock_spi, 0x300);
    ASSERT_EQ(mock_mem[0x300], 0xFF);
}

TEST(test_timeout_stuck_busy)
{
    mock_reset();
    mock_stuck_busy = 1;
    uint8_t data[] = {0x42};
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0, data, 1);
    ASSERT_EQ(e, W25QXX_ERR_TIMEOUT);
    mock_stuck_busy = 0;
}

TEST(test_null_safety)
{
    uint32_t id;
    uint8_t buf[4];
    uint8_t sr;
    ASSERT_EQ(w25qxx_init(NULL, &id), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_read(NULL, 0, buf, 4), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_read(&mock_spi, 0, NULL, 4), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_page_program(NULL, 0, buf, 1), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_page_program(&mock_spi, 0, NULL, 1), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_sector_erase(NULL, 0), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_chip_erase(NULL), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_read_status(NULL, &sr), W25QXX_ERR_SPI);
    ASSERT_EQ(w25qxx_read_status(&mock_spi, NULL), W25QXX_ERR_SPI);
}

TEST(test_reinit_different_chip)
{
    mock_reset();
    uint32_t id1 = 0;
    w25qxx_init(&mock_spi, &id1);
    ASSERT_EQ(id1, 0xEF4017);

    mock_jedec = 0xEF4018;
    uint32_t id2 = 0;
    w25qxx_err_t e = w25qxx_init(&mock_spi, &id2);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(id2, 0xEF4018);
}

TEST(test_read_address_zero)
{
    mock_reset();
    mock_mem[0] = 0x42;
    mock_mem[1] = 0x99;
    uint8_t buf[2] = {0};
    w25qxx_err_t e = w25qxx_read(&mock_spi, 0, buf, 2);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(buf[0], 0x42);
    ASSERT_EQ(buf[1], 0x99);
}

TEST(test_page_program_zero_len)
{
    mock_reset();
    uint8_t data = 0x55;
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0, &data, 0);
    ASSERT_EQ(e, W25QXX_ERR_ALIGN);
}

TEST(test_page_program_oversized)
{
    mock_reset();
    uint8_t data[257];
    memset(data, 0x55, sizeof(data));
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0, data, 257);
    ASSERT_EQ(e, W25QXX_ERR_ALIGN);
}

TEST(test_init_jedec_zero_manufacturer)
{
    mock_reset();
    mock_jedec = 0x004017;
    uint32_t id = 0;
    w25qxx_err_t e = w25qxx_init(&mock_spi, &id);
    ASSERT_EQ(e, W25QXX_ERR_JEDEC_ID);
}

TEST(test_page_program_end_of_page)
{
    mock_reset();
    uint8_t data = 0xAA;
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0x1FF, &data, 1);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(mock_mem[0x1FF], 0xAA);
    ASSERT_EQ(mock_mem[0x200], 0xFF);
}

TEST(test_sector_erase_non_aligned)
{
    mock_reset();
    mock_mem[0x1000] = 0x42;
    mock_mem[0x1800] = 0x42;
    mock_mem[0x1FFF] = 0x42;
    w25qxx_err_t e = w25qxx_sector_erase(&mock_spi, 0x1800);
    ASSERT_EQ(e, W25QXX_OK);
    ASSERT_EQ(mock_mem[0x1000], 0xFF);
    ASSERT_EQ(mock_mem[0x1800], 0xFF);
    ASSERT_EQ(mock_mem[0x1FFF], 0xFF);
}

/* Raw instructions, to see the mock's own rules without the driver */
static void raw_begin(uint8_t cmd)
{
    mock_cs_low(NULL);
    (void)mock_xfer(NULL, cmd);
}

static void raw_addr(uint32_t addr)
{
    (void)mock_xfer(NULL, (uint8_t)(addr >> 16));
    (void)mock_xfer(NULL, (uint8_t)(addr >> 8));
    (void)mock_xfer(NULL, (uint8_t)addr);
}

static uint8_t raw_status(void)
{
    raw_begin(0x05);
    uint8_t sr = mock_xfer(NULL, 0xFF);
    mock_cs_high(NULL);
    return sr;
}

static void raw_write_enable(void)
{
    raw_begin(0x06);
    mock_cs_high(NULL);
}

/* p.36: "After the Page Program cycle has finished the Write Enable Latch (WEL)
 * bit in the Status Register is cleared to 0." */
TEST(test_page_program_leaves_wel_clear)
{
    mock_reset();
    uint8_t data = 0x12;
    ASSERT_EQ(w25qxx_page_program(&mock_spi, 0, &data, 1), W25QXX_OK);

    uint8_t sr = 0xFF;
    ASSERT_EQ(w25qxx_read_status(&mock_spi, &sr), W25QXX_OK);
    ASSERT_EQ_HEX(sr, 0x00);
}

/* p.36: "The BUSY bit is a 1 during the Page Program cycle and becomes a 0 when
 * the cycle is finished" */
TEST(test_mock_is_busy_during_the_cycle)
{
    mock_reset();
    raw_write_enable();
    raw_begin(0x02);
    raw_addr(0x100);
    (void)mock_xfer(NULL, 0x12);
    mock_cs_high(NULL);

    ASSERT_EQ_HEX(raw_status() & SR1_BUSY, SR1_BUSY);
    while (raw_status() & SR1_BUSY) {
    }
    ASSERT_EQ_HEX(mock_mem[0x100], 0x12);
}

/* p.22: "all instructions except for Read Status Register will be ignored" */
TEST(test_mock_ignores_instructions_while_busy)
{
    mock_reset();
    mock_mem[0x100] = 0x42;
    raw_write_enable();
    raw_begin(0x20);
    raw_addr(0x100);
    mock_cs_high(NULL);

    raw_begin(0x03);
    raw_addr(0x100);
    ASSERT_EQ_HEX(mock_xfer(NULL, 0xFF), 0xFF);
    mock_cs_high(NULL);
    ASSERT_EQ_HEX(mock_mem[0x100], 0x42);
}

/* p.36 and p.41: nothing happens until /CS is driven high. */
TEST(test_mock_acts_only_when_cs_rises)
{
    mock_reset();
    mock_mem[0] = 0x42;

    raw_write_enable();
    raw_begin(0x02);
    raw_addr(0x100);
    (void)mock_xfer(NULL, 0x12);
    ASSERT_EQ_HEX(mock_mem[0x100], 0xFF);
    mock_cs_high(NULL);
    while (raw_status() & SR1_BUSY) {
    }

    raw_write_enable();
    raw_begin(0xC7);
    ASSERT_EQ_HEX(mock_mem[0], 0x42);
    mock_cs_high(NULL);
    while (raw_status() & SR1_BUSY) {
    }
    ASSERT_EQ_HEX(mock_mem[0], 0xFF);
}

/* p.36: "the addressing will wrap to the beginning of the page." */
TEST(test_mock_page_program_wraps_inside_the_page)
{
    mock_reset();
    raw_write_enable();
    raw_begin(0x02);
    raw_addr(0x1FF);
    (void)mock_xfer(NULL, 0x11);
    (void)mock_xfer(NULL, 0x22);
    mock_cs_high(NULL);
    while (raw_status() & SR1_BUSY) {
    }

    ASSERT_EQ_HEX(mock_mem[0x1FF], 0x11);
    ASSERT_EQ_HEX(mock_mem[0x100], 0x22);
    ASSERT_EQ_HEX(mock_mem[0x200], 0xFF);
}

/* ---------- main ---------- */

TEST_MAIN("w25qxx")
    RUN(test_init);
    RUN(test_init_bad_id);
    RUN(test_init_null_jedec_out);
    RUN(test_read);
    RUN(test_page_program);
    RUN(test_page_program_full_page);
    RUN(test_page_program_cross_boundary);
    RUN(test_sector_erase);
    RUN(test_chip_erase);
    RUN(test_read_write_cycle);
    RUN(test_read_status);
    RUN(test_nor_program_semantics);
    RUN(test_timeout_stuck_busy);
    RUN(test_null_safety);
    RUN(test_reinit_different_chip);
    RUN(test_read_address_zero);
    RUN(test_page_program_zero_len);
    RUN(test_page_program_oversized);
    RUN(test_init_jedec_zero_manufacturer);
    RUN(test_page_program_end_of_page);
    RUN(test_sector_erase_non_aligned);
    RUN(test_page_program_leaves_wel_clear);
    RUN(test_mock_is_busy_during_the_cycle);
    RUN(test_mock_ignores_instructions_while_busy);
    RUN(test_mock_acts_only_when_cs_rises);
    RUN(test_mock_page_program_wraps_inside_the_page);
TEST_MAIN_END()

