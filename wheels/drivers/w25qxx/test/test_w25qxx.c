#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../w25qxx.h"

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

static int     tests_run, tests_fail;

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

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)

/* ---------- tests ---------- */

static void test_init(void)
{
    printf("test_init\n");
    mock_reset();
    uint32_t id = 0;
    w25qxx_err_t e = w25qxx_init(&mock_spi, &id);
    ASSERT_EQ(e, W25QXX_OK, "init OK");
    ASSERT_EQ(id, 0xEF4017, "JEDEC ID = W25Q64");
}

static void test_init_bad_id(void)
{
    printf("test_init_bad_id\n");
    mock_reset();
    mock_jedec = 0xFF0000;
    uint32_t id = 0;
    w25qxx_err_t e = w25qxx_init(&mock_spi, &id);
    ASSERT_EQ(e, W25QXX_ERR_JEDEC_ID, "bad JEDEC detected");
}

static void test_read(void)
{
    printf("test_read\n");
    mock_reset();
    mock_mem[0x100] = 0xAB;
    mock_mem[0x101] = 0xCD;
    uint8_t buf[2] = {0};
    w25qxx_err_t e = w25qxx_read(&mock_spi, 0x100, buf, 2);
    ASSERT_EQ(e, W25QXX_OK, "read OK");
    ASSERT_EQ(buf[0], 0xAB, "byte 0");
    ASSERT_EQ(buf[1], 0xCD, "byte 1");
}

static void test_page_program(void)
{
    printf("test_page_program\n");
    mock_reset();
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0x200, data, 4);
    ASSERT_EQ(e, W25QXX_OK, "page program OK");
    ASSERT_EQ(mock_mem[0x200], 0x11, "written byte 0");
    ASSERT_EQ(mock_mem[0x203], 0x44, "written byte 3");
    ASSERT_EQ(mock_mem[0x204], 0xFF, "unwritten stays 0xFF");
}

static void test_page_program_cross_boundary(void)
{
    printf("test_page_program_cross_boundary\n");
    mock_reset();
    uint8_t data[2] = {0xAA, 0xBB};
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0xFF, data, 2);
    ASSERT_EQ(e, W25QXX_ERR_ALIGN, "cross-page rejected");
}

static void test_sector_erase(void)
{
    printf("test_sector_erase\n");
    mock_reset();
    mock_mem[0x1000] = 0x42;
    mock_mem[0x1FFF] = 0x42;
    mock_mem[0x2000] = 0x42;
    w25qxx_err_t e = w25qxx_sector_erase(&mock_spi, 0x1000);
    ASSERT_EQ(e, W25QXX_OK, "erase OK");
    ASSERT_EQ(mock_mem[0x1000], 0xFF, "erased byte");
    ASSERT_EQ(mock_mem[0x1FFF], 0xFF, "erased last byte");
    ASSERT_EQ(mock_mem[0x2000], 0x42, "next sector untouched");
}

static void test_chip_erase(void)
{
    printf("test_chip_erase\n");
    mock_reset();
    mock_mem[0] = 0x42;
    mock_mem[MOCK_SIZE - 1] = 0x42;
    w25qxx_err_t e = w25qxx_chip_erase(&mock_spi);
    ASSERT_EQ(e, W25QXX_OK, "chip erase OK");
    ASSERT_EQ(mock_mem[0], 0xFF, "first byte erased");
    ASSERT_EQ(mock_mem[MOCK_SIZE - 1], 0xFF, "last byte erased");
}

static void test_read_write_cycle(void)
{
    printf("test_read_write_cycle\n");
    mock_reset();
    w25qxx_sector_erase(&mock_spi, 0);

    uint8_t wr[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    w25qxx_page_program(&mock_spi, 0, wr, 8);

    uint8_t rd[8] = {0};
    w25qxx_read(&mock_spi, 0, rd, 8);
    ASSERT(memcmp(wr, rd, 8) == 0, "write-read roundtrip");
}

static void test_read_status(void)
{
    printf("test_read_status\n");
    mock_reset();
    uint8_t sr = 0xFF;
    w25qxx_err_t e = w25qxx_read_status(&mock_spi, &sr);
    ASSERT_EQ(e, W25QXX_OK, "read status OK");
    ASSERT_EQ(sr & 0x01, 0, "not busy");
}

static void test_init_null_jedec_out(void)
{
    printf("test_init_null_jedec_out\n");
    mock_reset();
    w25qxx_err_t e = w25qxx_init(&mock_spi, NULL);
    ASSERT_EQ(e, W25QXX_OK, "init with NULL jedec_id_out");
}

static void test_page_program_full_page(void)
{
    printf("test_page_program_full_page\n");
    mock_reset();
    uint8_t data[256];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0x100, data, 256);
    ASSERT_EQ(e, W25QXX_OK, "full page program OK");
    ASSERT_EQ(mock_mem[0x100], 0x00, "first byte");
    ASSERT_EQ(mock_mem[0x1FF], 0xFF, "last byte");
    ASSERT_EQ(mock_mem[0x200], 0xFF, "next page untouched");
}

static void test_nor_program_semantics(void)
{
    printf("test_nor_program_semantics\n");
    mock_reset();
    /* Write 0x0F first */
    uint8_t d1 = 0x0F;
    w25qxx_page_program(&mock_spi, 0x300, &d1, 1);
    ASSERT_EQ(mock_mem[0x300], 0x0F, "first write");

    /* Write 0xF0 on top — NOR can only clear bits, not set them */
    uint8_t d2 = 0xF0;
    w25qxx_page_program(&mock_spi, 0x300, &d2, 1);
    ASSERT_EQ(mock_mem[0x300], 0x00, "NOR: 0x0F & 0xF0 = 0x00");

    /* Erase restores 0xFF */
    w25qxx_sector_erase(&mock_spi, 0x300);
    ASSERT_EQ(mock_mem[0x300], 0xFF, "erase restores 0xFF");
}

static void test_timeout_stuck_busy(void)
{
    printf("test_timeout_stuck_busy\n");
    mock_reset();
    mock_stuck_busy = 1;
    uint8_t data[] = {0x42};
    w25qxx_err_t e = w25qxx_page_program(&mock_spi, 0, data, 1);
    ASSERT_EQ(e, W25QXX_ERR_TIMEOUT, "stuck busy returns timeout");
    mock_stuck_busy = 0;
}

/* ---------- main ---------- */

int main(void)
{
    test_init();
    test_init_bad_id();
    test_init_null_jedec_out();
    test_read();
    test_page_program();
    test_page_program_full_page();
    test_page_program_cross_boundary();
    test_sector_erase();
    test_chip_erase();
    test_read_write_cycle();
    test_read_status();
    test_nor_program_semantics();
    test_timeout_stuck_busy();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
