/*
 * test_at24cxx.c --- unit tests for at24cxx I2C EEPROM driver
 *
 * Uses an in-memory EEPROM mock (not mock_i2c.h, since AT24Cxx is
 * addressed memory rather than a register bank).
 */

#include "test_harness.h"
#include "at24cxx.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Mock EEPROM backend
 * ============================================================ */

#define MOCK_MEM_SIZE  32768u   /* large enough for AT24C256 */

static uint8_t mock_mem[MOCK_MEM_SIZE];
static int     mock_fail_next;
static int     mock_delay_calls;

static void mock_reset(void)
{
    memset(mock_mem, 0xFF, sizeof(mock_mem));
    mock_fail_next  = 0;
    mock_delay_calls = 0;
}

/* 8-bit address callbacks (AT24C01 .. AT24C16) */

static bool mock_write_reg(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t val)
{
    (void)ctx;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    /* For multi-page chips the upper address bits are in dev_addr */
    uint16_t base = (uint16_t)((dev_addr & 0x07u) << 8);
    uint16_t addr = (uint16_t)(base + reg);
    if (addr < MOCK_MEM_SIZE) mock_mem[addr] = val;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    uint16_t base = (uint16_t)((dev_addr & 0x07u) << 8);
    uint16_t addr = (uint16_t)(base + reg);
    for (uint16_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(addr + i);
        buf[i] = (a < MOCK_MEM_SIZE) ? mock_mem[a] : 0xFF;
    }
    return true;
}

/* 16-bit address callbacks (AT24C32 .. AT24C256) */

static bool mock_write_regs(void *ctx, uint8_t dev_addr,
                            uint16_t reg, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(reg + i);
        if (a < MOCK_MEM_SIZE) mock_mem[a] = data[i];
    }
    return true;
}

static bool mock_read_regs16(void *ctx, uint8_t dev_addr,
                             uint16_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(reg + i);
        buf[i] = (a < MOCK_MEM_SIZE) ? mock_mem[a] : 0xFF;
    }
    return true;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    mock_delay_calls++;
}

/* HAL instances */

static const at24cxx_i2c_t i2c_8bit = {
    .write_reg  = mock_write_reg,
    .read_regs  = mock_read_regs,
    .write_regs = NULL,
    .read_regs16 = NULL,
    .delay_ms   = mock_delay_ms,
    .ctx        = NULL,
};

static const at24cxx_i2c_t i2c_16bit = {
    .write_reg   = NULL,
    .read_regs   = NULL,
    .write_regs  = mock_write_regs,
    .read_regs16 = mock_read_regs16,
    .delay_ms    = mock_delay_ms,
    .ctx         = NULL,
};

static const at24cxx_i2c_t i2c_full = {
    .write_reg   = mock_write_reg,
    .read_regs   = mock_read_regs,
    .write_regs  = mock_write_regs,
    .read_regs16 = mock_read_regs16,
    .delay_ms    = mock_delay_ms,
    .ctx         = NULL,
};

/* ============================================================
 * Tests
 * ============================================================ */

TEST(test_init_valid)
{
    mock_reset();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02), AT24CXX_OK);
    ASSERT_EQ(dev.page_size, 8);
    ASSERT_EQ((int)dev.model, (int)AT24C02);
}

TEST(test_init_null_dev)
{
    mock_reset();
    ASSERT_EQ(at24cxx_init(NULL, &i2c_8bit, 0x50, AT24C02), AT24CXX_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    mock_reset();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, NULL, 0x50, AT24C02), AT24CXX_ERR_PARAM);
}

TEST(test_init_invalid_model)
{
    mock_reset();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &i2c_8bit, 0x50, (at24cxx_model_t)999),
              AT24CXX_ERR_PARAM);
}

TEST(test_init_16bit_missing_callbacks)
{
    mock_reset();
    at24cxx_t dev;
    /* 8-bit HAL lacks write_regs/read_regs16 -- must fail for AT24C32+ */
    ASSERT_EQ(at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C32), AT24CXX_ERR_PARAM);
}

TEST(test_read_single_byte)
{
    mock_reset();
    mock_mem[0x10] = 0xAB;
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    uint8_t val = 0;
    ASSERT_EQ(at24cxx_read(&dev, 0x10, &val, 1), AT24CXX_OK);
    ASSERT_EQ(val, 0xAB);
}

TEST(test_read_multiple_bytes)
{
    mock_reset();
    mock_mem[0] = 0x01;
    mock_mem[1] = 0x02;
    mock_mem[2] = 0x03;
    mock_mem[3] = 0x04;
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    uint8_t buf[4] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 0, buf, 4), AT24CXX_OK);
    ASSERT_EQ(buf[0], 0x01);
    ASSERT_EQ(buf[1], 0x02);
    ASSERT_EQ(buf[2], 0x03);
    ASSERT_EQ(buf[3], 0x04);
}

TEST(test_write_single_byte_readback)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    ASSERT_EQ(at24cxx_write_byte(&dev, 0x20, 0x5A), AT24CXX_OK);
    uint8_t val = 0;
    ASSERT_EQ(at24cxx_read(&dev, 0x20, &val, 1), AT24CXX_OK);
    ASSERT_EQ(val, 0x5A);
}

TEST(test_write_across_page_boundary)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    /* AT24C02 page_size=8.  Write 6 bytes starting at addr 5 --
     * crosses the 8-byte page boundary at addr 8. */
    uint8_t data[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
    mock_delay_calls = 0;
    ASSERT_EQ(at24cxx_write(&dev, 5, data, 6), AT24CXX_OK);
    /* Should have split into 2 page writes -> 2 delays */
    ASSERT_EQ(mock_delay_calls, 2);
    /* Verify data */
    uint8_t readback[6] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 5, readback, 6), AT24CXX_OK);
    ASSERT_EQ(readback[0], 0xA0);
    ASSERT_EQ(readback[1], 0xA1);
    ASSERT_EQ(readback[2], 0xA2);
    ASSERT_EQ(readback[3], 0xA3);
    ASSERT_EQ(readback[4], 0xA4);
    ASSERT_EQ(readback[5], 0xA5);
}

TEST(test_write_at_end_of_address_space)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    /* Write last byte of AT24C02 (addr 255) */
    ASSERT_EQ(at24cxx_write_byte(&dev, 255, 0xEE), AT24CXX_OK);
    uint8_t val = 0;
    ASSERT_EQ(at24cxx_read(&dev, 255, &val, 1), AT24CXX_OK);
    ASSERT_EQ(val, 0xEE);
}

TEST(test_address_out_of_range)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    uint8_t buf[1];
    /* AT24C02 = 256 bytes, reading at 256 is out of range */
    ASSERT_EQ(at24cxx_read(&dev, 256, buf, 1), AT24CXX_ERR_ADDR);
    /* Write past end */
    ASSERT_EQ(at24cxx_write_byte(&dev, 256, 0x00), AT24CXX_ERR_ADDR);
    /* Read that wraps past end */
    ASSERT_EQ(at24cxx_read(&dev, 250, buf, 10), AT24CXX_ERR_ADDR);
}

TEST(test_read_write_full_page)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    /* Write exactly one page (8 bytes) at a page-aligned address */
    uint8_t page[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    mock_delay_calls = 0;
    ASSERT_EQ(at24cxx_write(&dev, 0, page, 8), AT24CXX_OK);
    /* Exactly 1 page write -> 1 delay */
    ASSERT_EQ(mock_delay_calls, 1);
    uint8_t readback[8] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 0, readback, 8), AT24CXX_OK);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(readback[i], page[i]);
    }
}

TEST(test_roundtrip_pattern)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    /* Write a counting pattern across multiple pages */
    uint8_t pattern[32];
    for (int i = 0; i < 32; i++) pattern[i] = (uint8_t)i;
    ASSERT_EQ(at24cxx_write(&dev, 0, pattern, 32), AT24CXX_OK);
    uint8_t readback[32] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 0, readback, 32), AT24CXX_OK);
    for (int i = 0; i < 32; i++) {
        ASSERT_EQ(readback[i], (uint8_t)i);
    }
}

TEST(test_at24c256_16bit_addressing)
{
    mock_reset();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &i2c_16bit, 0x50, AT24C256), AT24CXX_OK);
    ASSERT_EQ(dev.page_size, 64);
    /* Write at high address (>255, requires 16-bit addressing) */
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(at24cxx_write(&dev, 0x1000, data, 4), AT24CXX_OK);
    uint8_t readback[4] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 0x1000, readback, 4), AT24CXX_OK);
    ASSERT_EQ(readback[0], 0xDE);
    ASSERT_EQ(readback[1], 0xAD);
    ASSERT_EQ(readback[2], 0xBE);
    ASSERT_EQ(readback[3], 0xEF);
}

TEST(test_at24c02_vs_at24c256_page_sizes)
{
    mock_reset();
    at24cxx_t dev02, dev256;
    at24cxx_init(&dev02,  &i2c_8bit,  0x50, AT24C02);
    at24cxx_init(&dev256, &i2c_16bit, 0x50, AT24C256);
    ASSERT_EQ(dev02.page_size, 8);
    ASSERT_EQ(dev256.page_size, 64);
    ASSERT_EQ((int)dev02.model, 256);
    ASSERT_EQ((int)dev256.model, 32768);
}

TEST(test_i2c_read_failure)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    uint8_t buf[1];
    mock_fail_next = 1;
    ASSERT_EQ(at24cxx_read(&dev, 0, buf, 1), AT24CXX_ERR_I2C);
}

TEST(test_i2c_write_failure)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    mock_fail_next = 1;
    ASSERT_EQ(at24cxx_write_byte(&dev, 0, 0x42), AT24CXX_ERR_I2C);
}

TEST(test_zero_length_operations)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, 0x50, AT24C02);
    uint8_t buf[1] = {0xFF};
    /* Zero-length read and write should succeed without I2C activity */
    ASSERT_EQ(at24cxx_read(&dev, 0, buf, 0), AT24CXX_OK);
    ASSERT_EQ(at24cxx_write(&dev, 0, buf, 0), AT24CXX_OK);
}

TEST(test_at24c04_address_bit_in_dev_addr)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_full, 0x50, AT24C04);
    /* AT24C04 = 512 bytes.  Addr 0x100 should encode bit8 into
     * the device address (0x50 | 0x01 = 0x51).  We write at 0x100
     * and verify mock_mem is written at offset 0x100. */
    ASSERT_EQ(at24cxx_write_byte(&dev, 0x100, 0x77), AT24CXX_OK);
    ASSERT_EQ(mock_mem[0x100], 0x77);
    uint8_t val = 0;
    ASSERT_EQ(at24cxx_read(&dev, 0x100, &val, 1), AT24CXX_OK);
    ASSERT_EQ(val, 0x77);
}

/* ============================================================
 * Main
 * ============================================================ */

TEST_MAIN("at24cxx")
    RUN(test_init_valid);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_invalid_model);
    RUN(test_init_16bit_missing_callbacks);
    RUN(test_read_single_byte);
    RUN(test_read_multiple_bytes);
    RUN(test_write_single_byte_readback);
    RUN(test_write_across_page_boundary);
    RUN(test_write_at_end_of_address_space);
    RUN(test_address_out_of_range);
    RUN(test_read_write_full_page);
    RUN(test_roundtrip_pattern);
    RUN(test_at24c256_16bit_addressing);
    RUN(test_at24c02_vs_at24c256_page_sizes);
    RUN(test_i2c_read_failure);
    RUN(test_i2c_write_failure);
    RUN(test_zero_length_operations);
    RUN(test_at24c04_address_bit_in_dev_addr);
TEST_MAIN_END()
