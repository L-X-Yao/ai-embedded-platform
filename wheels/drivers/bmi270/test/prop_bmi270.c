/*
 * prop_bmi270.c --- property-based tests for bmi270
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_bmi270.c ../bmi270.c -o prop_bmi270 -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../bmi270.h"
#include <string.h>
#include <math.h>

/* ---------- inline SPI mock (same protocol as existing test) ---------- */

#define MOCK_REG_COUNT 256
static uint8_t mock_regs_prop[MOCK_REG_COUNT];
static int     mock_op_count_prop;
static uint8_t mock_cs_state_prop;
static int     mock_byte_idx_prop;
static bool    mock_is_read_prop;
static uint8_t mock_addr_latch_prop;

static void prop_mock_reset(void) {
    memset(mock_regs_prop, 0, sizeof(mock_regs_prop));
    mock_op_count_prop  = 0;
    mock_cs_state_prop  = 1;
    mock_byte_idx_prop  = 0;
    mock_is_read_prop   = false;
    mock_addr_latch_prop = 0;

    /* POR defaults */
    mock_regs_prop[0x00] = 0x24;  /* CHIP_ID */
    mock_regs_prop[0x21] = 0x01;  /* INTERNAL_STATUS: init OK */
    mock_regs_prop[0x40] = 0x28;  /* ACC_CONF */
    mock_regs_prop[0x42] = 0x29;  /* GYR_CONF */
}

static void prop_cs_low(void *ctx) {
    (void)ctx;
    mock_cs_state_prop = 0;
    mock_byte_idx_prop = 0;
}

static void prop_cs_high(void *ctx) { (void)ctx; mock_cs_state_prop = 1; }

static uint8_t prop_xfer(void *ctx, uint8_t tx) {
    (void)ctx;
    mock_op_count_prop++;

    if (mock_byte_idx_prop == 0) {
        mock_is_read_prop = (tx & 0x80u) != 0;
        mock_addr_latch_prop = (uint8_t)(tx & 0x7Fu);
        mock_byte_idx_prop++;
        return 0x00;
    }

    if (mock_is_read_prop) {
        if (mock_byte_idx_prop == 1) {
            mock_byte_idx_prop++;
            return 0x00; /* dummy byte */
        }
        uint8_t offset = (uint8_t)(mock_byte_idx_prop - 2);
        uint8_t reg = (uint8_t)(mock_addr_latch_prop + offset);
        mock_byte_idx_prop++;
        return mock_regs_prop[reg];
    } else {
        if (mock_byte_idx_prop == 1) {
            mock_regs_prop[mock_addr_latch_prop] = tx;
        } else {
            uint8_t offset = (uint8_t)(mock_byte_idx_prop - 1);
            uint8_t reg = (uint8_t)(mock_addr_latch_prop + offset);
            mock_regs_prop[reg] = tx;
        }
        mock_byte_idx_prop++;
        return 0x00;
    }
}

static void prop_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const bmi270_spi_t prop_spi = {
    .cs_low   = prop_cs_low,
    .cs_high  = prop_cs_high,
    .xfer     = prop_xfer,
    .delay_ms = prop_delay,
    .ctx      = NULL,
};

/* Property: init does not crash with random register contents */
PROPERTY(init_no_crash_random_regs, 3000) {
    prop_mock_reset();

    /* Randomize non-critical registers */
    for (int i = 1; i < MOCK_REG_COUNT; i++) {
        if (i == 0x00 || i == 0x21) continue; /* keep CHIP_ID and INIT_STATUS */
        mock_regs_prop[i] = ARBI_UINT8(seed);
    }

    bmi270_t dev;
    memset(&dev, 0, sizeof(dev));
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_err_t rc = bmi270_init(&dev, &prop_spi, &cfg);
    PROP_ASSERT_EQ(rc, BMI270_OK);
}

/* Property: accel/gyro output is finite (not NaN/Inf) */
PROPERTY(accel_gyro_output_finite, 5000) {
    prop_mock_reset();

    /* Set random accel/gyro data registers */
    for (int r = 0x0C; r <= 0x17; r++) {
        mock_regs_prop[r] = ARBI_UINT8(seed);
    }

    bmi270_t dev;
    memset(&dev, 0, sizeof(dev));
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &prop_spi, &cfg);

    bmi270_accel_gyro_t data;
    bmi270_err_t rc = bmi270_read_accel_gyro(&dev, &data);
    PROP_ASSERT_EQ(rc, BMI270_OK);

    PROP_ASSERT(isfinite(data.ax_g));
    PROP_ASSERT(isfinite(data.ay_g));
    PROP_ASSERT(isfinite(data.az_g));
    PROP_ASSERT(isfinite(data.gx_dps));
    PROP_ASSERT(isfinite(data.gy_dps));
    PROP_ASSERT(isfinite(data.gz_dps));
}

/* Property: different config variants all init without error */
PROPERTY(config_variants, 1000) {
    prop_mock_reset();

    bmi270_t dev;
    memset(&dev, 0, sizeof(dev));

    bmi270_cfg_t cfg;
    cfg.acc_range  = (bmi270_acc_range_t)(ARBI_SIZE(seed, 3));
    cfg.gyro_range = (bmi270_gyro_range_t)(ARBI_SIZE(seed, 4));
    cfg.acc_odr    = (bmi270_odr_t)(0x06 + ARBI_SIZE(seed, 6));
    cfg.gyro_odr   = (bmi270_odr_t)(0x06 + ARBI_SIZE(seed, 6));

    bmi270_err_t rc = bmi270_init(&dev, &prop_spi, &cfg);
    PROP_ASSERT_EQ(rc, BMI270_OK);
}

/* Property: temperature read is finite */
PROPERTY(temp_read_finite, 3000) {
    prop_mock_reset();

    mock_regs_prop[0x22] = ARBI_UINT8(seed);
    mock_regs_prop[0x23] = ARBI_UINT8(seed);

    bmi270_t dev;
    memset(&dev, 0, sizeof(dev));
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &prop_spi, &cfg);

    float temp;
    bmi270_err_t rc = bmi270_read_temp(&dev, &temp);
    PROP_ASSERT_EQ(rc, BMI270_OK);
    PROP_ASSERT(isfinite(temp));
}

/* Property: default_cfg returns sensible values */
PROPERTY(default_cfg_sensible, 1) {
    bmi270_cfg_t cfg = bmi270_default_cfg();
    PROP_ASSERT(cfg.acc_range <= BMI270_ACC_RANGE_16G);
    PROP_ASSERT(cfg.gyro_range <= BMI270_GYRO_RANGE_125DPS);
    PROP_ASSERT(cfg.acc_odr >= BMI270_ODR_25HZ && cfg.acc_odr <= BMI270_ODR_1600HZ);
    PROP_ASSERT(cfg.gyro_odr >= BMI270_ODR_25HZ && cfg.gyro_odr <= BMI270_ODR_1600HZ);
}

TEST_MAIN("bmi270 properties") {
    RUN(init_no_crash_random_regs);
    RUN(accel_gyro_output_finite);
    RUN(config_variants);
    RUN(temp_read_finite);
    RUN(default_cfg_sensible);
}
TEST_MAIN_END()
