#include "test_harness.h"
#include "mock_i2c.h"
#include "vl53l0x.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* model ID register 0xC0 = 0xEE */
    mock_regs[0xC0] = 0xEE;
}

static const vl53l0x_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

/* ---------- helpers ---------- */

static vl53l0x_t dev;
static const vl53l0x_cfg_t default_cfg = {
    .addr = VL53L0X_ADDR_DEFAULT,
};

static void setup(void)
{
    mock_reset();
    vl53l0x_init(&dev, &mock_i2c, &default_cfg);
}

/** Plant a ready status + 16-bit range into mock registers. */
static void plant_range(uint16_t range_mm)
{
    /* RESULT_RANGE_STATUS (0x14) bit 0 = ready */
    mock_regs[0x14] = 0x01;
    /* range MSB at 0x14+10=0x1E, LSB at 0x1F */
    mock_regs[0x1E] = (uint8_t)(range_mm >> 8);
    mock_regs[0x1F] = (uint8_t)(range_mm & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    vl53l0x_t d;
    vl53l0x_err_t e = vl53l0x_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, VL53L0X_OK);
    ASSERT_EQ(d.timing_budget_us, 30000u);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0xC0] = 0x00;
    vl53l0x_t d;
    vl53l0x_err_t e = vl53l0x_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, VL53L0X_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    vl53l0x_t d;
    vl53l0x_err_t e = vl53l0x_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, VL53L0X_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(vl53l0x_init(NULL, &mock_i2c, &default_cfg), VL53L0X_ERR_PARAM);
    vl53l0x_t d;
    ASSERT_EQ(vl53l0x_init(&d, NULL, &default_cfg), VL53L0X_ERR_PARAM);
    ASSERT_EQ(vl53l0x_init(&d, &mock_i2c, NULL), VL53L0X_ERR_PARAM);
    uint16_t range;
    ASSERT_EQ(vl53l0x_read_range_mm(NULL, &range), VL53L0X_ERR_PARAM);
    ASSERT_EQ(vl53l0x_read_range_mm(&d, NULL), VL53L0X_ERR_PARAM);
    ASSERT_EQ(vl53l0x_start_single(NULL), VL53L0X_ERR_PARAM);
    bool ready;
    ASSERT_EQ(vl53l0x_is_ready(NULL, &ready), VL53L0X_ERR_PARAM);
    ASSERT_EQ(vl53l0x_is_ready(&d, NULL), VL53L0X_ERR_PARAM);
    ASSERT_EQ(vl53l0x_set_measurement_timing_budget(NULL, 30000),
              VL53L0X_ERR_PARAM);
}

TEST(test_read_range_ok)
{
    setup();
    plant_range(152);

    uint16_t range = 0;
    vl53l0x_err_t e = vl53l0x_read_range_mm(&dev, &range);
    ASSERT_EQ(e, VL53L0X_OK);
    ASSERT_EQ(range, 152);
}

TEST(test_read_range_max)
{
    setup();
    plant_range(2000);

    uint16_t range = 0;
    vl53l0x_err_t e = vl53l0x_read_range_mm(&dev, &range);
    ASSERT_EQ(e, VL53L0X_OK);
    ASSERT_EQ(range, 2000);
}

TEST(test_read_range_out_of_range)
{
    setup();
    /* 8190 (0x1FFE) = no target / out-of-range sentinel */
    plant_range(8190);

    uint16_t range = 0;
    vl53l0x_err_t e = vl53l0x_read_range_mm(&dev, &range);
    ASSERT_EQ(e, VL53L0X_ERR_RANGE);
}

TEST(test_read_range_timeout)
{
    setup();
    /* status bit 0 stays clear -> never ready */
    mock_regs[0x14] = 0x00;

    uint16_t range = 0;
    vl53l0x_err_t e = vl53l0x_read_range_mm(&dev, &range);
    ASSERT_EQ(e, VL53L0X_ERR_TIMEOUT);
}

TEST(test_read_range_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    uint16_t range = 0;
    vl53l0x_err_t e = vl53l0x_read_range_mm(&dev, &range);
    ASSERT_EQ(e, VL53L0X_ERR_I2C);
}

TEST(test_start_single)
{
    setup();
    vl53l0x_err_t e = vl53l0x_start_single(&dev);
    ASSERT_EQ(e, VL53L0X_OK);
    /* SYSRANGE_START (0x00) should be 0x01 */
    ASSERT_EQ(mock_regs[0x00], 0x01);
    /* interrupt clear (0x0B) should be 0x01 */
    ASSERT_EQ(mock_regs[0x0B], 0x01);
}

TEST(test_is_ready_true)
{
    setup();
    mock_regs[0x14] = 0x01;
    bool ready = false;
    vl53l0x_err_t e = vl53l0x_is_ready(&dev, &ready);
    ASSERT_EQ(e, VL53L0X_OK);
    ASSERT_TRUE(ready);
}

TEST(test_is_ready_false)
{
    setup();
    mock_regs[0x14] = 0x00;
    bool ready = true;
    vl53l0x_err_t e = vl53l0x_is_ready(&dev, &ready);
    ASSERT_EQ(e, VL53L0X_OK);
    ASSERT_FALSE(ready);
}

TEST(test_set_timing_budget_ok)
{
    setup();
    /* 50000 us -> encoded = 25000 = 0x61A8 (fits uint16) */
    vl53l0x_err_t e = vl53l0x_set_measurement_timing_budget(&dev, 50000);
    ASSERT_EQ(e, VL53L0X_OK);
    ASSERT_EQ(dev.timing_budget_us, 50000u);
    ASSERT_EQ(mock_regs[0x71], 0x61);
    ASSERT_EQ(mock_regs[0x72], 0xA8);
}

TEST(test_set_timing_budget_too_low)
{
    setup();
    vl53l0x_err_t e = vl53l0x_set_measurement_timing_budget(&dev, 10000);
    ASSERT_EQ(e, VL53L0X_ERR_PARAM);
}

TEST(test_set_timing_budget_too_high)
{
    setup();
    vl53l0x_err_t e = vl53l0x_set_measurement_timing_budget(&dev, 600000);
    ASSERT_EQ(e, VL53L0X_ERR_PARAM);
}

TEST(test_init_writes_sequence_config)
{
    mock_reset();
    vl53l0x_t d;
    vl53l0x_init(&d, &mock_i2c, &default_cfg);
    /* sequence config (0x01) should be 0xFF */
    ASSERT_EQ(mock_regs[0x01], 0xFF);
    /* MSRC config (0x60) should be 0x12 */
    ASSERT_EQ(mock_regs[0x60], 0x12);
}

TEST_MAIN("vl53l0x")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_range_ok);
    RUN(test_read_range_max);
    RUN(test_read_range_out_of_range);
    RUN(test_read_range_timeout);
    RUN(test_read_range_i2c_fail);
    RUN(test_start_single);
    RUN(test_is_ready_true);
    RUN(test_is_ready_false);
    RUN(test_set_timing_budget_ok);
    RUN(test_set_timing_budget_too_low);
    RUN(test_set_timing_budget_too_high);
    RUN(test_init_writes_sequence_config);
TEST_MAIN_END()
