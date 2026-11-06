// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "max31865.h"

/* ---------- mock SPI ---------- */

/* Register file: 8 registers (0x00..0x07) */
static uint8_t mock_regs[8];
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;
static uint8_t mock_cs_state;

static uint8_t xfer_log[256];
static int     xfer_log_idx;

/* SPI state machine for register read/write */
static int     mock_phase;     /* 0=address, 1=data */
static uint8_t mock_cur_addr;
static int     mock_is_write;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_next = 0;
    mock_op_count  = 0;
    mock_fail_at   = -1;
    mock_cs_state  = 1;
    mock_phase     = 0;
    mock_cur_addr  = 0;
    mock_is_write  = 0;
    xfer_log_idx   = 0;
    memset(xfer_log, 0, sizeof(xfer_log));
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs_state = 0;
    mock_phase = 0;  /* next xfer is address byte */
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_cs_state = 1;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    mock_op_count++;
    if (mock_fail_next) {
        mock_fail_next = 0;
        return 0xFF;
    }
    if (mock_fail_at >= 0 && mock_op_count > mock_fail_at) {
        return 0xFF;
    }

    if (xfer_log_idx < (int)sizeof(xfer_log))
        xfer_log[xfer_log_idx++] = tx;

    if (mock_phase == 0) {
        /* Address byte: bit7=1 means write */
        mock_is_write = (tx & 0x80) ? 1 : 0;
        mock_cur_addr = (uint8_t)(tx & 0x7F);
        mock_phase = 1;
        return 0xFF;
    }

    /* Data byte */
    if (mock_is_write) {
        if (mock_cur_addr < sizeof(mock_regs))
            mock_regs[mock_cur_addr] = tx;
        return 0xFF;
    } else {
        uint8_t val = 0xFF;
        if (mock_cur_addr < sizeof(mock_regs))
            val = mock_regs[mock_cur_addr];
        return val;
    }
}

static const max31865_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

static const max31865_cfg_t pt100_cfg = {
    .r_ref       = 430.0f,
    .r_nom       = 100.0f,
    .wire        = MAX31865_WIRE_2,
    .filter_50hz = false,
};

/* ---------- helpers ---------- */

static max31865_t dev;

static void setup(void)
{
    mock_reset();
    max31865_init(&dev, &mock_spi, &pt100_cfg);
}

/* Plant a raw 15-bit RTD value into registers 0x01-0x02.
 * The chip stores it as (rtd_raw << 1), fault bit in LSB bit 0. */
static void plant_rtd(uint16_t rtd_raw, uint8_t fault_bit)
{
    uint16_t reg_val = (uint16_t)((rtd_raw << 1) | (fault_bit & 1));
    mock_regs[0x01] = (uint8_t)(reg_val >> 8);
    mock_regs[0x02] = (uint8_t)(reg_val & 0xFF);
}

/* ---------- reference data from the manual ---------- */

/* Maxim 19-6478 Rev 3, Table 9 (p.20-21): PT100 with a 400 ohm RREF.
 * word is the 01h-02h register pair as printed. */
typedef struct {
    float    temp_c;
    uint16_t word;
} table9_row_t;

static const table9_row_t table9[] = {
    { -200.0f, 0x0BDA }, { -175.0f, 0x12B4 }, { -150.0f, 0x196C },
    { -125.0f, 0x200A }, { -100.0f, 0x2690 }, {  -75.0f, 0x2D04 },
    {  -50.0f, 0x3366 }, {  -40.0f, 0x35EE }, {  -30.0f, 0x3876 },
    {  -20.0f, 0x3AFC }, {  -10.0f, 0x3D7E },
    {   0.0f, 0x4000 }, {  10.0f, 0x4280 }, {  20.0f, 0x44FC },
    {  30.0f, 0x4778 }, {  40.0f, 0x49F2 }, {  50.0f, 0x4C6A },
    {  60.0f, 0x4EE0 }, {  70.0f, 0x5154 }, {  80.0f, 0x53C6 },
    {  90.0f, 0x5636 }, { 100.0f, 0x58A4 }, { 110.0f, 0x5B12 },
    { 120.0f, 0x5D7C }, { 130.0f, 0x5FE4 }, { 140.0f, 0x624C },
    { 150.0f, 0x64B0 }, { 160.0f, 0x6714 }, { 170.0f, 0x6974 },
    { 180.0f, 0x6BD4 }, { 190.0f, 0x6E30 }, { 200.0f, 0x708C },
    { 225.0f, 0x7668 }, { 250.0f, 0x7C3A }, { 275.0f, 0x81FE },
    { 300.0f, 0x87B6 }, { 325.0f, 0x8D64 }, { 350.0f, 0x9304 },
    { 375.0f, 0x989A }, { 400.0f, 0x9E24 }, { 425.0f, 0xA3A2 },
    { 450.0f, 0xA914 }, { 475.0f, 0xAE7A }, { 500.0f, 0xB3D4 },
    { 525.0f, 0xB922 }, { 550.0f, 0xBE64 },
};

/* Half an ADC code is 0.019 C at 550 C, the widest row of Table 9. */
#define TABLE9_TOL_C    0.02f

/* Codes at each end of the range: Table 9 at -200 C, and R(T) on p.10 at
 * 850 C, where the table has no row */
#define CODE_MINUS_200C 1517u
#define CODE_850C       31988u

/* R(T)/R0 from the equation on p.10, in double so that the check does not
 * share the driver's float rounding. */
static double cvd_ratio(double t)
{
    double ratio = 1.0 + 3.90830e-3 * t - 5.77500e-7 * t * t;

    if (t < 0.0)
        ratio += -4.18301e-12 * (t - 100.0) * t * t * t;
    return ratio;
}

static const max31865_cfg_t table9_cfg = {
    .r_ref       = 400.0f,
    .r_nom       = 100.0f,
    .wire        = MAX31865_WIRE_2,
    .filter_50hz = false,
};

/* p.10 pairs a PT1000 with 4k, which gives the same ratio and so the same
 * codes as Table 9. */
static const max31865_cfg_t pt1000_table9_cfg = {
    .r_ref       = 4000.0f,
    .r_nom       = 1000.0f,
    .wire        = MAX31865_WIRE_2,
    .filter_50hz = false,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    max31865_t d;
    max31865_err_t e = max31865_init(&d, &mock_spi, &pt100_cfg);
    ASSERT_EQ(e, MAX31865_OK);
    /* Config register should have bias + auto-conversion enabled */
    ASSERT_TRUE((mock_regs[0x00] & 0xC0) == 0xC0);
}

TEST(test_init_config_3wire)
{
    mock_reset();
    max31865_t d;
    max31865_cfg_t cfg3 = { .r_ref = 430.0f, .r_nom = 100.0f,
                            .wire = MAX31865_WIRE_3, .filter_50hz = false };
    max31865_err_t e = max31865_init(&d, &mock_spi, &cfg3);
    ASSERT_EQ(e, MAX31865_OK);
    /* 3-wire bit (0x10) should be set */
    ASSERT_TRUE((mock_regs[0x00] & 0x10) != 0);
}

TEST(test_init_config_50hz)
{
    mock_reset();
    max31865_t d;
    max31865_cfg_t cfg50 = { .r_ref = 430.0f, .r_nom = 100.0f,
                             .wire = MAX31865_WIRE_2, .filter_50hz = true };
    max31865_err_t e = max31865_init(&d, &mock_spi, &cfg50);
    ASSERT_EQ(e, MAX31865_OK);
    /* 50Hz filter bit (0x01) should be set */
    ASSERT_TRUE((mock_regs[0x00] & 0x01) != 0);
}

TEST(test_null_safety)
{
    max31865_t d;
    float temp;
    uint16_t raw;
    uint8_t fault;
    ASSERT_EQ(max31865_init(NULL, &mock_spi, &pt100_cfg), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_init(&d, NULL, &pt100_cfg), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_init(&d, &mock_spi, NULL), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_read_temperature(NULL, &temp), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_read_temperature(&d, NULL), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_read_rtd_raw(NULL, &raw), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_read_rtd_raw(&d, NULL), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_read_fault(NULL, &fault), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_read_fault(&d, NULL), MAX31865_ERR_PARAM);
    ASSERT_EQ(max31865_clear_fault(NULL), MAX31865_ERR_PARAM);
}

TEST(test_init_bad_rref)
{
    mock_reset();
    max31865_t d;
    max31865_cfg_t bad = { .r_ref = 0.0f, .r_nom = 100.0f,
                           .wire = MAX31865_WIRE_2, .filter_50hz = false };
    ASSERT_EQ(max31865_init(&d, &mock_spi, &bad), MAX31865_ERR_PARAM);
    bad.r_ref = -1.0f;
    ASSERT_EQ(max31865_init(&d, &mock_spi, &bad), MAX31865_ERR_PARAM);
}

TEST(test_init_bad_rnom)
{
    mock_reset();
    max31865_t d;
    max31865_cfg_t bad = { .r_ref = 430.0f, .r_nom = 0.0f,
                           .wire = MAX31865_WIRE_2, .filter_50hz = false };
    ASSERT_EQ(max31865_init(&d, &mock_spi, &bad), MAX31865_ERR_PARAM);
}

TEST(test_read_rtd_raw)
{
    setup();
    /* Plant RTD raw = 8192 (0x2000) → R = 8192/32768 * 430 = 107.5 ohm */
    plant_rtd(8192, 0);

    uint16_t raw = 0;
    max31865_err_t e = max31865_read_rtd_raw(&dev, &raw);
    ASSERT_EQ(e, MAX31865_OK);
    ASSERT_EQ(raw, 8192);
}

TEST(test_read_rtd_fault_bit)
{
    setup();
    /* Fault bit set in LSB → should return ERR_FAULT */
    plant_rtd(8192, 1);

    uint16_t raw = 0;
    max31865_err_t e = max31865_read_rtd_raw(&dev, &raw);
    ASSERT_EQ(e, MAX31865_ERR_FAULT);
}

TEST(test_read_temperature_0c)
{
    setup();
    /* At 0C: R_rtd = R_nom = 100 ohm
     * RTD raw = (100/430) * 32768 = 7620.93 ≈ 7621 */
    plant_rtd(7621, 0);

    float temp = 999.0f;
    max31865_err_t e = max31865_read_temperature(&dev, &temp);
    ASSERT_EQ(e, MAX31865_OK);
    ASSERT_NEAR(temp, 0.0f, 1.0f);
}

TEST(test_read_temperature_100c)
{
    setup();
    /* At 100C: R = 100*(1 + A*100 + B*100^2) = 138.51 ohm
     * RTD raw = (138.51/430) * 32768 = 10554 */
    plant_rtd(10554, 0);

    float temp = 0.0f;
    max31865_err_t e = max31865_read_temperature(&dev, &temp);
    ASSERT_EQ(e, MAX31865_OK);
    ASSERT_NEAR(temp, 100.0f, 2.0f);
}

TEST(test_read_temperature_negative)
{
    setup();
    /* At ~ -20C: R ≈ 92.16 ohm
     * RTD raw = (92.16/430) * 32768 = 7022 */
    plant_rtd(7022, 0);

    float temp = 0.0f;
    max31865_err_t e = max31865_read_temperature(&dev, &temp);
    ASSERT_EQ(e, MAX31865_OK);
    ASSERT_TRUE(temp < 0.0f);
    ASSERT_NEAR(temp, -20.0f, 3.0f);
}

TEST(test_read_temperature_pt1000)
{
    mock_reset();
    max31865_t d;
    max31865_cfg_t pt1000_cfg = { .r_ref = 4300.0f, .r_nom = 1000.0f,
                                  .wire = MAX31865_WIRE_2, .filter_50hz = false };
    max31865_init(&d, &mock_spi, &pt1000_cfg);

    /* At 0C: R = 1000 ohm, RTD raw = (1000/4300)*32768 = 7621 */
    plant_rtd(7621, 0);

    float temp = 999.0f;
    max31865_err_t e = max31865_read_temperature(&d, &temp);
    ASSERT_EQ(e, MAX31865_OK);
    ASSERT_NEAR(temp, 0.0f, 1.0f);
}

static void check_table9(const max31865_cfg_t *cfg)
{
    mock_reset();
    ASSERT_EQ(max31865_init(&dev, &mock_spi, cfg), MAX31865_OK);

    for (size_t i = 0; i < sizeof(table9) / sizeof(table9[0]); i++) {
        float temp = 9999.0f;
        plant_rtd((uint16_t)(table9[i].word >> 1),
                  (uint8_t)(table9[i].word & 1u));
        ASSERT_EQ(max31865_read_temperature(&dev, &temp), MAX31865_OK);
        ASSERT_NEAR(temp, table9[i].temp_c, TABLE9_TOL_C);
    }
}

TEST(test_table9_rows)
{
    check_table9(&table9_cfg);
}

TEST(test_table9_rows_pt1000)
{
    check_table9(&pt1000_table9_cfg);
}

/* Table 9 stops at 550 C, so the top of the range comes from the equation. */
TEST(test_equation_up_to_850c)
{
    mock_reset();
    ASSERT_EQ(max31865_init(&dev, &mock_spi, &table9_cfg), MAX31865_OK);

    for (int t = 575; t <= 850; t += 25) {
        float temp = 9999.0f;
        double code = cvd_ratio((double)t) * 100.0 / 400.0 * 32768.0;
        plant_rtd((uint16_t)(code + 0.5), 0);
        ASSERT_EQ(max31865_read_temperature(&dev, &temp), MAX31865_OK);
        /* half an ADC code is 0.021 C at 850 C */
        ASSERT_NEAR(temp, (float)t, 0.025f);
    }
}

/* Every code in range must solve the equation and rank above the last one,
 * which also catches a step where two branches meet. */
TEST(test_every_code_solves_equation)
{
    mock_reset();
    ASSERT_EQ(max31865_init(&dev, &mock_spi, &table9_cfg), MAX31865_OK);

    float prev = -1000.0f;
    for (uint16_t code = CODE_MINUS_200C; code <= CODE_850C; code++) {
        float temp = 9999.0f;
        plant_rtd(code, 0);
        ASSERT_EQ(max31865_read_temperature(&dev, &temp), MAX31865_OK);
        /* one ADC code is 0.012 ohm */
        ASSERT_NEAR_D(cvd_ratio((double)temp) * 100.0,
                      (double)code * 400.0 / 32768.0, 0.0002);
        ASSERT_TRUE(temp > prev);
        prev = temp;
    }
}

TEST(test_read_fault_register)
{
    setup();
    mock_regs[0x07] = 0xA4;  /* high-thresh + REFIN high + OV/UV */

    uint8_t fault = 0;
    max31865_err_t e = max31865_read_fault(&dev, &fault);
    ASSERT_EQ(e, MAX31865_OK);
    ASSERT_EQ(fault, 0xA4);
    ASSERT_TRUE(fault & MAX31865_FAULT_HIGHTHRESH);
    ASSERT_TRUE(fault & MAX31865_FAULT_OVUV);
}

TEST(test_clear_fault)
{
    setup();
    mock_regs[0x07] = 0x80;  /* some fault present */

    max31865_err_t e = max31865_clear_fault(&dev);
    ASSERT_EQ(e, MAX31865_OK);

    /* After clear_fault, config register should have auto-conversion
     * re-enabled (0x40) and fault-clear bit (0x02) should be cleared */
    uint8_t cfg = mock_regs[0x00];
    ASSERT_TRUE((cfg & 0x40) != 0);   /* auto still on */
    ASSERT_TRUE((cfg & 0x02) == 0);   /* fault-clear bit cleared */
}

TEST(test_init_spi_null_callbacks)
{
    max31865_t d;
    max31865_spi_t bad_spi = { .cs_low = NULL, .cs_high = mock_cs_high,
                               .xfer = mock_xfer, .ctx = NULL };
    ASSERT_EQ(max31865_init(&d, &bad_spi, &pt100_cfg), MAX31865_ERR_SPI);

    bad_spi.cs_low = mock_cs_low;
    bad_spi.cs_high = NULL;
    ASSERT_EQ(max31865_init(&d, &bad_spi, &pt100_cfg), MAX31865_ERR_SPI);

    bad_spi.cs_high = mock_cs_high;
    bad_spi.xfer = NULL;
    ASSERT_EQ(max31865_init(&d, &bad_spi, &pt100_cfg), MAX31865_ERR_SPI);
}

/* ---------- main ---------- */

/* p.13: the RTD registers reset to 00h, and p.16 sets the fault flag on any
 * result at or below the POR low threshold, so a clear word is no result */
TEST(test_read_before_the_first_conversion)
{
    setup();
    plant_rtd(0, 0);

    uint16_t raw = 77;
    float temp = 77.0f;
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_ERR_NOT_READY);
    ASSERT_EQ(max31865_read_temperature(&dev, &temp), MAX31865_ERR_NOT_READY);
    ASSERT_EQ(raw, 77);
    ASSERT_NEAR(temp, 77.0f, 0.001f);
}

/* Code 0 with the fault flag is a finished conversion of a shorted RTD */
TEST(test_code_zero_with_the_flag_is_a_fault)
{
    setup();
    plant_rtd(0, 1);

    uint16_t raw;
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_ERR_FAULT);
}

/* The smallest result is a result */
TEST(test_code_one_is_a_result)
{
    setup();
    plant_rtd(1, 0);

    uint16_t raw = 0;
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_OK);
    ASSERT_EQ(raw, 1);
}

TEST_MAIN("max31865")
    RUN(test_init_ok);
    RUN(test_init_config_3wire);
    RUN(test_init_config_50hz);
    RUN(test_null_safety);
    RUN(test_init_bad_rref);
    RUN(test_init_bad_rnom);
    RUN(test_read_rtd_raw);
    RUN(test_read_rtd_fault_bit);
    RUN(test_read_temperature_0c);
    RUN(test_read_temperature_100c);
    RUN(test_read_temperature_negative);
    RUN(test_read_temperature_pt1000);
    RUN(test_table9_rows);
    RUN(test_table9_rows_pt1000);
    RUN(test_equation_up_to_850c);
    RUN(test_every_code_solves_equation);
    RUN(test_read_fault_register);
    RUN(test_clear_fault);
    RUN(test_init_spi_null_callbacks);
    RUN(test_read_before_the_first_conversion);
    RUN(test_code_zero_with_the_flag_is_a_fault);
    RUN(test_code_one_is_a_result);
TEST_MAIN_END()
