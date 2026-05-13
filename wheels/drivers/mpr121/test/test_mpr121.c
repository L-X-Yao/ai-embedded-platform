#include "test_harness.h"
#include "mock_i2c.h"
#include "mpr121.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* After soft reset, ECR must read 0x00 */
    mock_regs[MPR121_REG_ECR] = 0x00u;
}

static const mpr121_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static const mpr121_cfg_t default_cfg = {
    .touch_threshold   = 12,
    .release_threshold = 6,
    .ecr               = 0x8Fu,
};

static mpr121_t dev;

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(mpr121_init(&dev, &mock_i2c, &default_cfg), MPR121_OK);
    /* Verify ECR was written with configured value */
    ASSERT_EQ(mock_regs[MPR121_REG_ECR], 0x8Fu);
}

TEST(test_init_soft_reset_write)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);
    /* Verify soft reset was written: 0x63 to reg 0x80 */
    ASSERT_EQ(mock_regs[MPR121_REG_SOFT_RESET], 0x63u);
}

TEST(test_init_thresholds_written)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);
    /* Check all 12 electrode thresholds */
    for (int ch = 0; ch < 12; ch++) {
        uint8_t tth_reg = (uint8_t)(MPR121_REG_E0TTH + (uint8_t)ch * 2u);
        uint8_t rth_reg = (uint8_t)(MPR121_REG_E0TTH + (uint8_t)ch * 2u + 1u);
        ASSERT_EQ(mock_regs[tth_reg], 12);
        ASSERT_EQ(mock_regs[rth_reg], 6);
    }
}

TEST(test_init_delay_called)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);
    /* delay_ms must be called at least once (after soft reset) */
    ASSERT_TRUE(mock_delay_ms_count >= 1u);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* first I2C op (soft reset write) fails */
    ASSERT_EQ(mpr121_init(&dev, &mock_i2c, &default_cfg), MPR121_ERR_I2C);
}

TEST(test_init_ecr_verify_fail)
{
    mock_reset();
    /* ECR reads non-zero after reset → verification fails */
    mock_regs[MPR121_REG_ECR] = 0x42u;
    ASSERT_EQ(mpr121_init(&dev, &mock_i2c, &default_cfg), MPR121_ERR_I2C);
}

/* ---------- NULL safety (basic, non-aborting with CONTRACT_ENABLED) ---------- */
/* Full NULL parameter tests are in test_null_mpr121.c */

/* ---------- read_touched ---------- */

TEST(test_read_touched_no_touch)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    mock_regs[MPR121_REG_TOUCH_STATUS_L] = 0x00u;
    mock_regs[MPR121_REG_TOUCH_STATUS_H] = 0x00u;

    uint16_t status = 0xFFFFu;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_OK);
    ASSERT_EQ(status, 0u);
}

TEST(test_read_touched_single_channel)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    /* Touch on electrode 3 (bit 3) */
    mock_regs[MPR121_REG_TOUCH_STATUS_L] = 0x08u;
    mock_regs[MPR121_REG_TOUCH_STATUS_H] = 0x00u;

    uint16_t status = 0;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_OK);
    ASSERT_EQ_HEX(status, 0x0008u);
}

TEST(test_read_touched_multiple_channels)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    /* Touch on electrodes 0, 5, 11 */
    mock_regs[MPR121_REG_TOUCH_STATUS_L] = 0x21u;  /* bit 0 + bit 5 */
    mock_regs[MPR121_REG_TOUCH_STATUS_H] = 0x08u;  /* bit 11 */

    uint16_t status = 0;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_OK);
    ASSERT_EQ_HEX(status, 0x0821u);
}

TEST(test_read_touched_all_channels)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    mock_regs[MPR121_REG_TOUCH_STATUS_L] = 0xFFu;
    mock_regs[MPR121_REG_TOUCH_STATUS_H] = 0x0Fu;

    uint16_t status = 0;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_OK);
    ASSERT_EQ_HEX(status, 0x0FFFu);
}

TEST(test_read_touched_i2c_fail)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    mock_fail_next = 1;
    uint16_t status = 0;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_ERR_I2C);
}

/* ---------- read_electrode ---------- */

TEST(test_read_electrode_ch0)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    /* Electrode 0: regs 0x04 (low), 0x05 (high) */
    mock_regs[0x04] = 0x34u;  /* low byte  */
    mock_regs[0x05] = 0x01u;  /* high byte (bits 9:8) */

    uint16_t val = 0;
    ASSERT_EQ(mpr121_read_electrode(&dev, 0, &val), MPR121_OK);
    ASSERT_EQ(val, 0x0134u);  /* 10-bit: 0x134 = 308 */
}

TEST(test_read_electrode_ch11)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    /* Electrode 11: regs 0x1A (low), 0x1B (high) */
    mock_regs[0x1A] = 0xFFu;
    mock_regs[0x1B] = 0x03u;

    uint16_t val = 0;
    ASSERT_EQ(mpr121_read_electrode(&dev, 11, &val), MPR121_OK);
    ASSERT_EQ(val, 0x03FFu);  /* max 10-bit value */
}

TEST(test_read_electrode_10bit_mask)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    /* Write upper bits beyond 10-bit range; they must be masked */
    mock_regs[0x04] = 0xFFu;
    mock_regs[0x05] = 0xFFu;  /* only bits 1:0 should be used */

    uint16_t val = 0;
    ASSERT_EQ(mpr121_read_electrode(&dev, 0, &val), MPR121_OK);
    ASSERT_EQ(val, 0x03FFu);  /* masked to 10 bits */
}

TEST(test_read_electrode_invalid_channel)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    uint16_t val = 0;
    ASSERT_EQ(mpr121_read_electrode(&dev, 12, &val), MPR121_ERR_PARAM);
    ASSERT_EQ(mpr121_read_electrode(&dev, 255, &val), MPR121_ERR_PARAM);
}

TEST(test_read_electrode_i2c_fail)
{
    mock_reset();
    mpr121_init(&dev, &mock_i2c, &default_cfg);

    mock_fail_next = 1;
    uint16_t val = 0;
    ASSERT_EQ(mpr121_read_electrode(&dev, 0, &val), MPR121_ERR_I2C);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)MPR121_OK);
    ASSERT_EQ(-1, (int)MPR121_ERR_I2C);
    ASSERT_EQ(-2, (int)MPR121_ERR_PARAM);
}

TEST_MAIN("mpr121")
    RUN(test_init_ok);
    RUN(test_init_soft_reset_write);
    RUN(test_init_thresholds_written);
    RUN(test_init_delay_called);
    RUN(test_init_i2c_fail);
    RUN(test_init_ecr_verify_fail);
    RUN(test_read_touched_no_touch);
    RUN(test_read_touched_single_channel);
    RUN(test_read_touched_multiple_channels);
    RUN(test_read_touched_all_channels);
    RUN(test_read_touched_i2c_fail);
    RUN(test_read_electrode_ch0);
    RUN(test_read_electrode_ch11);
    RUN(test_read_electrode_10bit_mask);
    RUN(test_read_electrode_invalid_channel);
    RUN(test_read_electrode_i2c_fail);
    RUN(test_error_codes);
TEST_MAIN_END()
