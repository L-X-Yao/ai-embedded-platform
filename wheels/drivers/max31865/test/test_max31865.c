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
    RUN(test_read_fault_register);
    RUN(test_clear_fault);
    RUN(test_init_spi_null_callbacks);
TEST_MAIN_END()
