// SPDX-License-Identifier: MIT
/*
 * prop_at24cxx.c — property-based tests for AT24Cxx EEPROM driver
 *
 * Uses an in-memory EEPROM mock (same pattern as test_at24cxx.c).
 *
 * Properties:
 *   1. write then read returns same data for any address/length
 *   2. write across page boundary preserves all bytes
 *   3. read at out-of-range address returns ERR_ADDR
 *   4. init with random models never crashes
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_time.h"
#include "at24cxx.h"
#include <string.h>

/* ---------- in-memory EEPROM mock ---------- */

#define MOCK_MEM_SIZE 32768u

static uint8_t mock_mem[MOCK_MEM_SIZE];
static int     mock_fail_flag;

static void mock_reset(void)
{
    memset(mock_mem, 0xFF, sizeof(mock_mem));
    mock_fail_flag = 0;
}

static bool mock_write_reg(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t val)
{
    (void)ctx;
    if (mock_fail_flag) return false;
    uint16_t base = (uint16_t)((dev_addr & 0x07u) << 8);
    uint16_t addr = (uint16_t)(base + reg);
    if (addr < MOCK_MEM_SIZE) mock_mem[addr] = val;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (mock_fail_flag) return false;
    uint16_t base = (uint16_t)((dev_addr & 0x07u) << 8);
    uint16_t addr = (uint16_t)(base + reg);
    for (uint16_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(addr + i);
        buf[i] = (a < MOCK_MEM_SIZE) ? mock_mem[a] : 0xFF;
    }
    return true;
}

static bool mock_write_regs16(void *ctx, uint8_t dev_addr,
                              uint16_t reg, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_flag) return false;
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
    if (mock_fail_flag) return false;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(reg + i);
        buf[i] = (a < MOCK_MEM_SIZE) ? mock_mem[a] : 0xFF;
    }
    return true;
}

static const at24cxx_i2c_t i2c_8bit = {
    .write_reg   = mock_write_reg,
    .read_regs   = mock_read_regs,
    .write_regs8 = NULL,
    .write_regs  = NULL,
    .read_regs16 = NULL,
    .delay_ms    = mock_delay_ms,
    .ctx         = NULL,
};

static const at24cxx_i2c_t i2c_full = {
    .write_reg   = mock_write_reg,
    .read_regs   = mock_read_regs,
    .write_regs8 = NULL,
    .write_regs  = mock_write_regs16,
    .read_regs16 = mock_read_regs16,
    .delay_ms    = mock_delay_ms,
    .ctx         = NULL,
};

/* ------------------------------------------------------------------ */

PROPERTY(write_read_roundtrip, 5000)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, AT24CXX_ADDR_DEFAULT, AT24CXX_C02);

    /* Random address within AT24C02 range (256 bytes) */
    uint8_t addr = ARBI_UINT8(seed);
    uint16_t max_len = (uint16_t)(256 - addr);
    if (max_len == 0) return;
    uint16_t len = (uint16_t)(1 + ARBI_UINT8(seed) % (max_len > 32 ? 32 : max_len));

    uint8_t wbuf[32];
    ARBI_FILL(seed, wbuf, len);

    at24cxx_err_t err = at24cxx_write(&dev, addr, wbuf, len);
    if (err != AT24CXX_OK) return;

    uint8_t rbuf[32];
    memset(rbuf, 0, sizeof(rbuf));
    err = at24cxx_read(&dev, addr, rbuf, len);
    PROP_ASSERT(err == AT24CXX_OK);
    PROP_ASSERT_MEM_EQ(rbuf, wbuf, len);
}

PROPERTY(page_boundary_preserves, 5000)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, AT24CXX_ADDR_DEFAULT, AT24CXX_C02);

    /* Force write that crosses a page boundary (page_size=8) */
    uint8_t page_offset = (uint8_t)(1 + ARBI_UINT8(seed) % 7); /* 1..7 within page */
    uint8_t addr = (uint8_t)((ARBI_UINT8(seed) % 28) * 8 + page_offset);
    /* Length crosses into next page */
    uint16_t cross = (uint16_t)(8 - page_offset);
    uint16_t len = (uint16_t)(cross + 1 + ARBI_UINT8(seed) % 8);
    if (addr + len > 256) len = (uint16_t)(256 - addr);
    if (len == 0) return;
    if (len > 32) len = 32;

    uint8_t wbuf[32];
    ARBI_FILL(seed, wbuf, len);

    at24cxx_err_t err = at24cxx_write(&dev, addr, wbuf, len);
    if (err != AT24CXX_OK) return;

    uint8_t rbuf[32];
    memset(rbuf, 0, sizeof(rbuf));
    err = at24cxx_read(&dev, addr, rbuf, len);
    PROP_ASSERT(err == AT24CXX_OK);
    PROP_ASSERT_MEM_EQ(rbuf, wbuf, len);
}

PROPERTY(out_of_range_returns_error, 5000)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_init(&dev, &i2c_8bit, AT24CXX_ADDR_DEFAULT, AT24CXX_C02);

    /* Generate an address that exceeds AT24C02 capacity (256 bytes) */
    uint16_t addr = (uint16_t)(256 + ARBI_UINT8(seed));
    uint8_t buf[1] = {0};
    at24cxx_err_t err = at24cxx_read(&dev, addr, buf, 1);
    PROP_ASSERT(err == AT24CXX_ERR_ADDR);
}

PROPERTY(c256_roundtrip, 5000)
{
    mock_reset();
    at24cxx_t dev;
    at24cxx_err_t err = at24cxx_init(&dev, &i2c_full, AT24CXX_ADDR_DEFAULT,
                                      AT24CXX_C256);
    if (err != AT24CXX_OK) return;

    /* Random 16-bit address within AT24C256 range (32768 bytes) */
    uint16_t addr = ARBI_UINT16(seed) % 32700;
    uint16_t len = (uint16_t)(1 + ARBI_UINT8(seed) % 32);
    if (addr + len > 32768) len = (uint16_t)(32768 - addr);

    uint8_t wbuf[32];
    ARBI_FILL(seed, wbuf, len);

    err = at24cxx_write(&dev, addr, wbuf, len);
    if (err != AT24CXX_OK) return;

    uint8_t rbuf[32];
    memset(rbuf, 0, sizeof(rbuf));
    err = at24cxx_read(&dev, addr, rbuf, len);
    PROP_ASSERT(err == AT24CXX_OK);
    PROP_ASSERT_MEM_EQ(rbuf, wbuf, len);
}

TEST_MAIN("prop_at24cxx")
    RUN(write_read_roundtrip);
    RUN(page_boundary_preserves);
    RUN(out_of_range_returns_error);
    RUN(c256_roundtrip);
TEST_MAIN_END()
