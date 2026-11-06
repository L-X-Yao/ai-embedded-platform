// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ms5611.h"
#include <math.h>

/*
 * MS5611 uses command-based I2C. We abuse mock_regs:
 * - Reset and Convert land in bus_log as the exact bytes after the address
 * - read_regs(addr, 0x00, buf, 3) reads ADC data from mock_regs[0..2], and only
 *   after a Convert: p11 "If the conversion is not executed before the ADC read
 *   command, or the ADC read command is repeated, it will give 0"
 * - read_regs(addr, 0xA2..0xAC, buf, 2) reads PROM from mock_regs[prom_addr..+1]
 *
 * Plant PROM calibration values from the MS5611 datasheet example:
 *   C1=40127, C2=36924, C3=23317, C4=23282, C5=33464, C6=28312
 */

/*
 * PROM vector from ArduPilot libraries/SITL/SIM_MS5611.h (commit 32f57526):
 * the C1-C6 of the manual's p8 table, word 0 = 0xFFFF, word 7 = 0x0008. It
 * passes AP_Baro_MS5611.cpp's check, and Figure 16 (p13) puts the CRC in
 * DB3-DB0 of word 7. The vector is literal so that no test computes the
 * CRC with a copy of the driver's own routine.
 */
static const uint16_t ref_prom[8] = {
    0xFFFF,  /* word 0 - reserved for manufacturer */
    40127,   /* C1 = 0x9CBF */
    36924,   /* C2 = 0x903C */
    23317,   /* C3 = 0x5B15 */
    23282,   /* C4 = 0x5AF2 */
    33464,   /* C5 = 0x82B8 */
    28312,   /* C6 = 0x6E98 */
    0x0008,  /* word 7 - CRC 0x8 in DB3-DB0 */
};

static void plant_word7(uint16_t word7)
{
    mock_regs[0xAE] = (uint8_t)(word7 >> 8);
    mock_regs[0xAF] = (uint8_t)(word7 & 0xFF);
}

static void plant_prom(void)
{
    /* Word n sits at 0xA0 + 2n, MSB first */
    for (uint8_t n = 0; n < 8; n++)
    {
        uint8_t reg = (uint8_t)(0xA0u + 2u * n);
        mock_regs[reg]      = (uint8_t)(ref_prom[n] >> 8);
        mock_regs[reg + 1u] = (uint8_t)(ref_prom[n] & 0xFF);
    }
}

static void plant_adc(uint32_t val)
{
    mock_regs[0x00] = (uint8_t)(val >> 16);
    mock_regs[0x01] = (uint8_t)(val >> 8);
    mock_regs[0x02] = (uint8_t)(val);
}

/*
 * Every I2C write lands in bus_log whichever HAL callback carried it, so a
 * test sees what the chip would see. MS5611-01BA03 06/2017 p10: a command is
 * 1 byte. Figures 10 and 13: the message ends right after it.
 */
#define BUS_LOG_MAX 8
#define BUS_TX_MAX  4

typedef struct {
    uint8_t  addr;
    uint16_t len;
    uint8_t  data[BUS_TX_MAX];
} bus_tx_t;

static bus_tx_t bus_log[BUS_LOG_MAX];
static int      bus_count;
static bool     adc_ready;   /* a Convert went out since the last ADC read */

static void bus_clear(void)
{
    memset(bus_log, 0, sizeof(bus_log));
    bus_count = 0;
}

static bool bus_write(uint8_t dev_addr, const uint8_t *data, uint16_t len)
{
    if (mock_i2c_should_fail()) return false;

    if (bus_count < BUS_LOG_MAX) {
        bus_tx_t *tx = &bus_log[bus_count];
        tx->addr = dev_addr;
        tx->len  = len;
        memcpy(tx->data, data, len < BUS_TX_MAX ? len : BUS_TX_MAX);
    }
    bus_count++;

    /* p10: Convert D1 is 0x40-0x48, Convert D2 is 0x50-0x58 */
    if (len == 1 && (data[0] & 0xE0u) == 0x40u)
        adc_ready = true;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    if (!mock_i2c_read_regs(ctx, dev_addr, reg, buf, len))
        return false;

    if (reg == 0x00) {
        if (!adc_ready)
            memset(buf, 0, len);
        adc_ready = false;
    }
    return true;
}

static bool bus_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    const uint8_t tx[2] = { reg, val };

    (void)ctx;
    return bus_write(dev_addr, tx, 2);
}

static bool bus_write_raw(void *ctx, uint8_t dev_addr, const uint8_t *data,
                          uint16_t len)
{
    (void)ctx;
    return bus_write(dev_addr, data, len);
}

/* Assert that write transaction i was exactly these bytes */
#define ASSERT_TX(i, ...) do { \
    static const uint8_t want_[] = { __VA_ARGS__ }; \
    ASSERT_TRUE((i) < bus_count); \
    ASSERT_EQ(bus_log[i].len, sizeof(want_)); \
    ASSERT_MEMEQ_N(bus_log[i].data, want_, sizeof(want_)); \
} while (0)

static void mock_reset(void)
{
    mock_i2c_reset();
    bus_clear();
    adc_ready = false;
    plant_prom();
}

static const ms5611_i2c_t mock_i2c = {
    .write_reg = bus_write_reg,
    .read_regs = mock_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
    .write_raw = bus_write_raw,
};

static ms5611_t dev;
static ms5611_cfg_t default_cfg = {
    .addr = MS5611_ADDR_CSB_LOW,
    .osr  = MS5611_OSR_4096,
};

static void setup(void)
{
    mock_reset();
    ms5611_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* Reset command fails */
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ms5611_init(NULL, &mock_i2c, &default_cfg), MS5611_ERR_PARAM);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, NULL, &default_cfg), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, NULL), MS5611_ERR_PARAM);
    ms5611_data_t data;
    ASSERT_EQ(ms5611_read(NULL, &data), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_read(&d, NULL), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_reset(NULL), MS5611_ERR_PARAM);
}

TEST(test_init_requires_delay)
{
    /* p11: "If the ADC read command is sent during conversion the result
     * will be 0, the conversion will not stop and the final result will
     * be wrong." Only the delay callback keeps reads out of that window. */
    mock_reset();
    ms5611_i2c_t no_delay = mock_i2c;
    no_delay.delay_ms = NULL;
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, &no_delay, &default_cfg), MS5611_ERR_PARAM);
}

TEST(test_init_refuses_a_hal_without_write_raw)
{
    ms5611_i2c_t hal = mock_i2c;
    ms5611_t d;

    hal.write_raw = NULL;
    mock_reset();
    ASSERT_EQ(ms5611_init(&d, &hal, &default_cfg), MS5611_ERR_PARAM);
    /* nothing on the bus at all, reads included */
    ASSERT_EQ(mock_op_count, 0);
    ASSERT_EQ(bus_count, 0);
}

TEST(test_init_needs_no_write_reg)
{
    ms5611_i2c_t hal = mock_i2c;
    ms5611_t d;

    hal.write_reg = NULL;
    mock_reset();
    ASSERT_EQ(ms5611_init(&d, &hal, &default_cfg), MS5611_OK);
    ASSERT_TX(0, 0x1E);
}

TEST(test_prom_readback)
{
    setup();
    ASSERT_EQ(dev.calib.c1, 40127);
    ASSERT_EQ(dev.calib.c2, 36924);
    ASSERT_EQ(dev.calib.c3, 23317);
    ASSERT_EQ(dev.calib.c4, 23282);
    ASSERT_EQ(dev.calib.c5, 33464);
    ASSERT_EQ(dev.calib.c6, 28312);
}

TEST(test_read_ok)
{
    setup();

    /*
     * With register-based mock, both D1 and D2 read the same ADC value.
     * Plant a value that produces valid (non-NaN) output with the example
     * calibration data. D1=D2=8569150 (0x82BE9E).
     */
    plant_adc(8569150);

    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_OK);
    /* Verify outputs are finite (not NaN) */
    ASSERT_TRUE(data.temperature_c == data.temperature_c);
    ASSERT_TRUE(data.pressure_pa == data.pressure_pa);
}

TEST(test_read_compensation)
{
    setup();

    /*
     * Verify compensation produces plausible temperature from known ADC.
     * D2 = C5*256 + 256 => dT = 256 => temp ~ 2000 + tiny = ~20C
     * D1 = same value (mock limitation).
     * ADC = 33464*256 + 256 = 8567040.
     */
    plant_adc(8567040);

    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_OK);
    /* Temperature near 20C (dT is small => temp ~ 2000/100 = 20) */
    ASSERT_TRUE(data.temperature_c > 19.0f && data.temperature_c < 21.0f);
}

TEST(test_read_i2c_fail_d1)
{
    setup();
    /* D1 convert command fails (first op after init) */
    mock_fail_next = 1;
    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    ms5611_err_t e = ms5611_reset(&dev);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ms5611_err_t e = ms5611_reset(&dev);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_addr_csb_high)
{
    mock_reset();
    ms5611_cfg_t cfg = {
        .addr = MS5611_ADDR_CSB_HIGH,
        .osr  = MS5611_OSR_4096,
    };
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, MS5611_OK);
    ASSERT_EQ(bus_log[0].addr, MS5611_ADDR_CSB_HIGH);
}

TEST(test_prom_read_i2c_fail)
{
    mock_reset();
    /* Reset succeeds (op 0), first PROM read fails (op 1) */
    mock_fail_at = 1;
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_osr_invalid)
{
    mock_reset();
    ms5611_cfg_t cfg = {
        .addr = MS5611_ADDR_CSB_LOW,
        .osr  = (ms5611_osr_t)99,
    };
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, MS5611_ERR_PARAM);
}

/* p.11: 0 is the only way the part reports a missed conversion */
TEST(test_read_refuses_an_adc_result_of_0)
{
    setup();
    plant_adc(0);
    ms5611_data_t data;
    ASSERT_EQ(ms5611_read(&dev, &data), MS5611_ERR_ADC);
}

/* The mock itself, p.11 "or the ADC read command is repeated" */
TEST(test_mock_gives_each_conversion_once)
{
    static const uint8_t convert = 0x48;
    uint8_t adc[3] = { 0xFF, 0xFF, 0xFF };

    setup();
    plant_adc(0x123456);
    ASSERT_TRUE(mock_read_regs(NULL, MS5611_ADDR_CSB_LOW, 0x00, adc, 3));
    ASSERT_EQ_HEX(adc[0], 0x00);   /* no Convert yet */

    ASSERT_TRUE(bus_write_raw(NULL, MS5611_ADDR_CSB_LOW, &convert, 1));
    ASSERT_TRUE(mock_read_regs(NULL, MS5611_ADDR_CSB_LOW, 0x00, adc, 3));
    ASSERT_EQ_HEX(adc[0], 0x12);
    ASSERT_EQ_HEX(adc[1], 0x34);
    ASSERT_EQ_HEX(adc[2], 0x56);

    ASSERT_TRUE(mock_read_regs(NULL, MS5611_ADDR_CSB_LOW, 0x00, adc, 3));
    ASSERT_EQ_HEX(adc[0], 0x00);   /* repeated */
    ASSERT_EQ_HEX(adc[1], 0x00);
    ASSERT_EQ_HEX(adc[2], 0x00);
}

TEST(test_delay_called)
{
    setup();
    uint32_t before = mock_delay_ms_count;
    plant_adc(9085466);
    ms5611_data_t data;
    ms5611_read(&dev, &data);
    /* At least 2 delays: one for D1, one for D2 */
    ASSERT_TRUE(mock_delay_ms_count > before + 1);
}

TEST(test_init_crc_fail)
{
    /* Flip one bit of the CRC, which Figure 16 puts in DB3-DB0 of word 7 */
    mock_reset();
    plant_word7(ref_prom[7] ^ 0x0001u);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_ERR_CRC);
}

TEST(test_init_crc_is_not_in_top_nibble)
{
    /* The layout the driver used to expect: 0x8 in DB15-DB12 */
    mock_reset();
    plant_word7(0x8000);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_ERR_CRC);
}

TEST(test_init_crc_skips_word7_db7_db4)
{
    /* The reference clears the low byte (prom[7] &= 0xff00), so DB7-DB4
     * do not count: the CRC of word 7 = 0x00F8 is still 0x8. */
    mock_reset();
    plant_word7(0x00F8);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_OK);
}

TEST(test_init_crc_covers_word7_top_byte)
{
    /* The top byte does count. Reference crc_crc4 gives 0x3 for 0xA5xx */
    mock_reset();
    plant_word7(0xA503);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_OK);

    mock_reset();
    plant_word7(0xA403);   /* same CRC, other top byte: 0xA4xx gives 0x0 */
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_ERR_CRC);
}

TEST(test_init_reset_is_one_byte)
{
    ms5611_t d;

    mock_reset();
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    /* Figure 10: address, command 0x1E, STOP */
    ASSERT_EQ(bus_count, 1);
    ASSERT_TX(0, 0x1E);
    ASSERT_EQ(bus_log[0].addr, MS5611_ADDR_CSB_LOW);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_reset_is_one_byte)
{
    setup();
    bus_clear();
    ASSERT_EQ(ms5611_reset(&dev), MS5611_OK);
    ASSERT_EQ(bus_count, 1);
    ASSERT_TX(0, 0x1E);
}

TEST(test_convert_is_one_byte_at_every_osr)
{
    /* p10 command table, OSR 256 to 4096 */
    static const uint8_t d1[5] = { 0x40, 0x42, 0x44, 0x46, 0x48 };
    static const uint8_t d2[5] = { 0x50, 0x52, 0x54, 0x56, 0x58 };
    ms5611_data_t data;
    ms5611_t d;

    for (int osr = MS5611_OSR_256; osr <= MS5611_OSR_4096; osr++) {
        ms5611_cfg_t cfg = {
            .addr = MS5611_ADDR_CSB_LOW,
            .osr  = (ms5611_osr_t)osr,
        };

        mock_reset();
        ASSERT_EQ(ms5611_init(&d, &mock_i2c, &cfg), MS5611_OK);
        bus_clear();
        plant_adc(8569150);
        ASSERT_EQ(ms5611_read(&d, &data), MS5611_OK);
        /* Figure 13: address, one command byte, STOP */
        ASSERT_EQ(bus_count, 2);
        ASSERT_EQ(bus_log[0].len, 1);
        ASSERT_EQ_HEX(bus_log[0].data[0], d1[osr]);
        ASSERT_EQ(bus_log[1].len, 1);
        ASSERT_EQ_HEX(bus_log[1].data[0], d2[osr]);
    }
}

TEST(test_init_refuses_an_all_zero_prom)
{
    /* A bus that reads zeros. The CRC of zeros is 0, so the CRC passes it */
    ms5611_t d;

    mock_reset();
    memset(&mock_regs[0xA0], 0, 16);
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_ERR_CRC);
}

TEST(test_init_zero_check_counts_word0)
{
    /* Not all zero. Reference crc_crc4 gives 0x0 for word 0 = 0x0013 */
    ms5611_t d;

    mock_reset();
    memset(&mock_regs[0xA0], 0, 16);
    mock_regs[0xA1] = 0x13;
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_OK);
}

TEST(test_init_zero_check_counts_word7)
{
    /* Not all zero. Reference crc_crc4 gives 0x3 for a top byte of 0x01 */
    ms5611_t d;

    mock_reset();
    memset(&mock_regs[0xA0], 0, 16);
    plant_word7(0x0103);
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, &default_cfg), MS5611_OK);
}

TEST_MAIN("ms5611")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_init_requires_delay);
    RUN(test_init_refuses_a_hal_without_write_raw);
    RUN(test_init_needs_no_write_reg);
    RUN(test_prom_readback);
    RUN(test_read_ok);
    RUN(test_read_compensation);
    RUN(test_read_i2c_fail_d1);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_csb_high);
    RUN(test_prom_read_i2c_fail);
    RUN(test_osr_invalid);
    RUN(test_delay_called);
    RUN(test_read_refuses_an_adc_result_of_0);
    RUN(test_mock_gives_each_conversion_once);
    RUN(test_init_crc_fail);
    RUN(test_init_crc_is_not_in_top_nibble);
    RUN(test_init_crc_skips_word7_db7_db4);
    RUN(test_init_crc_covers_word7_top_byte);
    RUN(test_init_reset_is_one_byte);
    RUN(test_reset_is_one_byte);
    RUN(test_convert_is_one_byte_at_every_osr);
    RUN(test_init_refuses_an_all_zero_prom);
    RUN(test_init_zero_check_counts_word0);
    RUN(test_init_zero_check_counts_word7);
TEST_MAIN_END()
