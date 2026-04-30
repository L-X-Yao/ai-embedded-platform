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
}

static void mock_cs_low(void *ctx) { (void)ctx; mock_cs = 0; mock_phase = 1; mock_addr_bytes = 0; }
static void mock_cs_high(void *ctx) {
    (void)ctx;
    if (mock_cmd == 0x02 && mock_phase >= 3) mock_status &= ~0x01;
    if (mock_cmd == 0x20 && (mock_status & 0x02)) {
        uint32_t base = mock_addr & ~0xFFFU;
        uint32_t end = base + 4096;
        if (end > MOCK_SIZE) end = MOCK_SIZE;
        for (uint32_t i = base; i < end; i++) mock_mem[i] = 0xFF;
        mock_status &= ~0x03;
    }
    if (mock_cmd == 0xC7) mock_status &= ~0x01;
    mock_cs = 1;
    mock_phase = 0;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_fail_next) { mock_fail_next = 0; return 0xFF; }

    if (mock_phase == 1) {
        mock_cmd = tx;
        mock_addr = 0;
        mock_addr_bytes = 0;

        if (tx == 0x06) { mock_status |= 0x02; return 0xFF; } /* WREN → set WEL */
        if (tx == 0x9F) { mock_phase = 3; return 0xFF; }       /* JEDEC ID */
        if (tx == 0x05) { mock_phase = 3; return 0xFF; }       /* Read SR1 */
        if (tx == 0xC7) {                                       /* Chip erase */
            if (mock_status & 0x02) {
                memset(mock_mem, 0xFF, MOCK_SIZE);
                mock_status &= ~0x02;
            }
            return 0xFF;
        }
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
        if (mock_cmd == 0x05) return mock_status | (mock_stuck_busy ? SR1_BUSY : 0);
        if (mock_cmd == 0x03) {
            uint8_t r = (mock_addr < MOCK_SIZE) ? mock_mem[mock_addr] : 0xFF;
            mock_addr++;
            return r;
        }
        if (mock_cmd == 0x02) {
            if ((mock_status & 0x02) && mock_addr < MOCK_SIZE)
                mock_mem[mock_addr] &= tx; /* NOR flash: can only clear bits */
            mock_addr++;
            return 0xFF;
        }
        if (mock_cmd == 0x20) {
            if (mock_status & 0x02) {
                uint32_t base = mock_addr & ~0xFFFU;
                uint32_t end = base + 4096;
                if (end > MOCK_SIZE) end = MOCK_SIZE;
                for (uint32_t i = base; i < end; i++) mock_mem[i] = 0xFF;
                mock_status &= ~0x02;
            }
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
TEST_MAIN_END()

