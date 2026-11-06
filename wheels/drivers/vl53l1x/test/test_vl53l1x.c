// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_vl53l1x.h"
#include "vl53l1x.h"

/* ---------- hw_model setup ---------- */

static vl53l1x_model_t model;

static const vl53l1x_i2c_t hw_i2c = {
    .write_reg = vl53l1x_model_write_reg,
    .read_regs = vl53l1x_model_read_regs,
    .delay_ms  = vl53l1x_model_delay_ms,
    .ctx       = &model,
};

static const vl53l1x_cfg_t default_cfg = {
    .addr = VL53L1X_ADDR_DEFAULT,
};

static vl53l1x_t dev;

static void setup(void)
{
    vl53l1x_model_init(&model);
}

static void setup_inited(void)
{
    setup();
    vl53l1x_init(&dev, &hw_i2c, &default_cfg);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    setup();
    vl53l1x_t d;
    vl53l1x_err_t err = vl53l1x_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_FALSE(d.ranging);
}

TEST(test_init_wrong_model_id)
{
    setup();
    model.regs[VL1X_REG_MODEL_ID] = 0x00;
    vl53l1x_t d;
    vl53l1x_err_t err = vl53l1x_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L1X_ERR_ID);
}

TEST(test_init_wrong_module_type)
{
    setup();
    model.regs[VL1X_REG_MODULE_TYPE] = 0x00;
    vl53l1x_t d;
    vl53l1x_err_t err = vl53l1x_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L1X_ERR_ID);
}

TEST(test_init_i2c_fail_on_model_id)
{
    setup();
    model.fail_next = true;
    vl53l1x_t d;
    vl53l1x_err_t err = vl53l1x_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L1X_ERR_I2C);
}

TEST(test_init_i2c_fail_on_module_type)
{
    setup();
    /* First read (model ID) succeeds, second (module type) fails */
    vl53l1x_t d;
    /* Do the init manually: the model's fail_next is consumed on next op */
    d.i2c = &hw_i2c;
    d.cfg = default_cfg;
    /* Read model ID succeeds */
    uint8_t id = 0;
    hw_i2c.read_regs(hw_i2c.ctx, default_cfg.addr, 0x010F, &id, 1);
    ASSERT_EQ(id, 0xEA);
    /* Now set fail for the module type read */
    model.fail_next = true;
    vl53l1x_err_t err = vl53l1x_init(&d, &hw_i2c, &default_cfg);
    /* init reads model ID (ok), then module type (fail) */
    ASSERT_EQ(err, VL53L1X_ERR_I2C);
}

/* ---------- NULL safety ---------- */

TEST(test_null_safety)
{
    ASSERT_EQ(vl53l1x_init(NULL, &hw_i2c, &default_cfg), VL53L1X_ERR_PARAM);

    vl53l1x_t d;
    ASSERT_EQ(vl53l1x_init(&d, NULL, &default_cfg), VL53L1X_ERR_PARAM);
    ASSERT_EQ(vl53l1x_init(&d, &hw_i2c, NULL), VL53L1X_ERR_PARAM);

    uint16_t dist;
    ASSERT_EQ(vl53l1x_read_distance(NULL, &dist), VL53L1X_ERR_PARAM);
    ASSERT_EQ(vl53l1x_start_ranging(NULL), VL53L1X_ERR_PARAM);
    ASSERT_EQ(vl53l1x_stop_ranging(NULL), VL53L1X_ERR_PARAM);
}

TEST(test_null_out_pointer)
{
    setup_inited();
    ASSERT_EQ(vl53l1x_read_distance(&dev, NULL), VL53L1X_ERR_PARAM);
}

TEST(test_zero_init_hal)
{
    vl53l1x_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    vl53l1x_t d;
    ASSERT_TRUE(vl53l1x_init(&d, &zero_hal, &default_cfg) < 0);
}

/* ---------- read distance ---------- */

TEST(test_read_distance_default)
{
    setup_inited();
    /* hw_model POR: range registers hold default true_range_mm = 250 */
    vl53l1x_model_inject_distance(&model, 250);

    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 250);
}

TEST(test_read_distance_zero)
{
    setup_inited();
    vl53l1x_model_inject_distance(&model, 0);

    uint16_t dist = 9999;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 0);
}

TEST(test_read_distance_max)
{
    setup_inited();
    vl53l1x_model_inject_distance(&model, 4000);

    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 4000);
}

TEST(test_read_distance_large)
{
    setup_inited();
    vl53l1x_model_inject_distance(&model, 0xFFFF);

    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 0xFFFF);
}

TEST(test_read_distance_i2c_fail)
{
    setup_inited();
    model.fail_next = true;
    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_ERR_I2C);
}

/* ---------- start / stop ranging ---------- */

TEST(test_start_ranging)
{
    setup_inited();
    vl53l1x_err_t err = vl53l1x_start_ranging(&dev);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_TRUE(dev.ranging);
    /* System start register should contain 0x40 */
    ASSERT_EQ(model.regs[VL1X_REG_SYSTEM_START], 0x40);
}

TEST(test_stop_ranging)
{
    setup_inited();
    vl53l1x_start_ranging(&dev);
    vl53l1x_err_t err = vl53l1x_stop_ranging(&dev);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_FALSE(dev.ranging);
    /* System start register should contain 0x00 */
    ASSERT_EQ(model.regs[VL1X_REG_SYSTEM_START], 0x00);
}

TEST(test_start_ranging_i2c_fail)
{
    setup_inited();
    model.fail_next = true;
    vl53l1x_err_t err = vl53l1x_start_ranging(&dev);
    ASSERT_EQ(err, VL53L1X_ERR_I2C);
}

TEST(test_stop_ranging_i2c_fail)
{
    setup_inited();
    vl53l1x_start_ranging(&dev);
    model.fail_next = true;
    vl53l1x_err_t err = vl53l1x_stop_ranging(&dev);
    ASSERT_EQ(err, VL53L1X_ERR_I2C);
}

/* ---------- error code values ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)VL53L1X_OK);
    ASSERT_EQ(-1, (int)VL53L1X_ERR_I2C);
    ASSERT_EQ(-2, (int)VL53L1X_ERR_ID);
    ASSERT_EQ(-3, (int)VL53L1X_ERR_PARAM);
}

/* ---------- ranging populates distance via hw_model engine ---------- */

TEST(test_start_ranging_populates_distance)
{
    setup_inited();
    model.true_range_mm = 1500;
    vl53l1x_start_ranging(&dev);

    /* After start_ranging, the model populates the result registers */
    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 1500);
}

TEST(test_measure_count_increments)
{
    setup_inited();
    int before = model.measure_count;
    vl53l1x_start_ranging(&dev);
    ASSERT_EQ(model.measure_count, before + 1);
}

/* ---------- hw_model readonly enforcement ---------- */

TEST(test_model_id_readonly)
{
    setup();
    /* Attempt to overwrite the model ID register via HAL write */
    vl53l1x_model_write_reg(&model, 0x29, VL1X_REG_MODEL_ID, 0x00);
    ASSERT_EQ(model.regs[VL1X_REG_MODEL_ID], 0xEA);
}

TEST(test_result_regs_readonly)
{
    setup_inited();
    vl53l1x_model_inject_distance(&model, 1234);

    /* Attempt to overwrite result registers via HAL write */
    vl53l1x_model_write_reg(&model, 0x29, VL1X_REG_RESULT_RANGE_HI, 0xFF);
    vl53l1x_model_write_reg(&model, 0x29, VL1X_REG_RESULT_RANGE_LO, 0xFF);

    /* Should still read 1234 */
    uint16_t dist = 0;
    vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(dist, 1234);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(vl53l1x_t *d)
{
    uint16_t dist_mm;
    ASSERT_EQ(vl53l1x_read_distance(d, &dist_mm), VL53L1X_ERR_PARAM);
    ASSERT_EQ(vl53l1x_start_ranging(d), VL53L1X_ERR_PARAM);
    ASSERT_EQ(vl53l1x_stop_ranging(d), VL53L1X_ERR_PARAM);
}

TEST(test_device_refused_after_wrong_id)
{
    vl53l1x_t d;
    setup();
    model.regs[VL1X_REG_MODEL_ID] = 0x00;
    ASSERT_EQ(vl53l1x_init(&d, &hw_i2c, &default_cfg), VL53L1X_ERR_ID);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const vl53l1x_i2c_t zeroed;
    vl53l1x_t d;
    setup();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(vl53l1x_init(&d, NULL, &default_cfg), VL53L1X_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(vl53l1x_init(&d, &zeroed, &default_cfg), VL53L1X_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("vl53l1x")
    RUN(test_init_ok);
    RUN(test_init_wrong_model_id);
    RUN(test_init_wrong_module_type);
    RUN(test_init_i2c_fail_on_model_id);
    RUN(test_init_i2c_fail_on_module_type);
    RUN(test_null_safety);
    RUN(test_null_out_pointer);
    RUN(test_zero_init_hal);
    RUN(test_read_distance_default);
    RUN(test_read_distance_zero);
    RUN(test_read_distance_max);
    RUN(test_read_distance_large);
    RUN(test_read_distance_i2c_fail);
    RUN(test_start_ranging);
    RUN(test_stop_ranging);
    RUN(test_start_ranging_i2c_fail);
    RUN(test_stop_ranging_i2c_fail);
    RUN(test_error_codes);
    RUN(test_start_ranging_populates_distance);
    RUN(test_measure_count_increments);
    RUN(test_model_id_readonly);
    RUN(test_result_regs_readonly);
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
