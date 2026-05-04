/*
 * test_hw_model_hmc5883l.c — tests using hardware register model
 *
 * Validates HMC5883L driver against a datasheet-accurate register model.
 * The model enforces read-only ID registers ('H','4','3'), data register
 * protection, config register writable masks, and measurement trigger.
 */

#include "test_harness.h"
#include "hw_model_hmc5883l.h"
#include "../hmc5883l.h"
#include <math.h>

static hmc5883l_model_t model;

static const hmc5883l_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const hmc5883l_cfg_t default_cfg = {
    .gain = HMC5883L_GAIN_1090,
    .rate = HMC5883L_RATE_15,
};

static void setup(void)
{
    hmc5883l_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_err_t err = hmc5883l_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, HMC5883L_OK);
}

TEST(test_init_wrong_id)
{
    setup();
    /* Corrupt 'H' -> 'X' */
    model.hw.reset_values[HMC5883L_REG_ID_A] = 'X';
    hw_model_reset(&model.hw);

    hmc5883l_t dev;
    hmc5883l_err_t err = hmc5883l_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, HMC5883L_ERR_ID);
}

TEST(test_init_sets_config_a)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &model_i2c, &cfg);
    /* Config A: rate=4 shifted left by 2 => 0x10 */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_CONFIG_A], 0x10u);
}

TEST(test_init_sets_config_b)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &model_i2c, &cfg);
    /* Config B: gain=1 shifted left by 5 => 0x20 */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_CONFIG_B], 0x20u);
}

TEST(test_init_sets_continuous_mode)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);
    /* Mode register: 0x00 = continuous measurement */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_MODE], 0x00u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_raw_values)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);

    hmc5883l_raw_t raw;
    hmc5883l_err_t err = hmc5883l_read_raw(&dev, &raw);
    ASSERT_EQ(err, HMC5883L_OK);
    ASSERT_EQ(raw.x, model.true_x);
    ASSERT_EQ(raw.y, model.true_y);
    ASSERT_EQ(raw.z, model.true_z);
}

TEST(test_read_gauss_reasonable)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);

    hmc5883l_data_t data;
    hmc5883l_err_t err = hmc5883l_read_gauss(&dev, &data);
    ASSERT_EQ(err, HMC5883L_OK);
    /* true_x=500, scale=1/1090 -> ~0.459 Gauss */
    ASSERT_TRUE(data.x_gauss > 0.3f && data.x_gauss < 0.6f);
    /* true_y=-200 -> negative */
    ASSERT_TRUE(data.y_gauss < 0.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_id_registers_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, HMC5883L_REG_ID_A, 0xFF);
    hw_model__write_reg(&model.hw, HMC5883L_REG_ID_B, 0xFF);
    hw_model__write_reg(&model.hw, HMC5883L_REG_ID_C, 0xFF);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_ID_A], 'H');
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_ID_B], '4');
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_ID_C], '3');
}

TEST(test_data_registers_readonly)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);

    uint8_t orig = model.hw.regs[HMC5883L_REG_DATA_X_H];
    hw_model__write_reg(&model.hw, HMC5883L_REG_DATA_X_H, 0xFF);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_DATA_X_H], orig);
}

TEST(test_config_b_writable_mask)
{
    setup();
    /* Config B only has bits [7:5] writable */
    hw_model__write_reg(&model.hw, HMC5883L_REG_CONFIG_B, 0xFF);
    /* Only high 3 bits should be set */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_CONFIG_B], 0xE0u);
}

TEST(test_idle_mode)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_MODE], 0x00u);

    hmc5883l_idle(&dev);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_MODE], 0x02u);
}

TEST(test_different_field_strengths)
{
    setup();
    model.true_x = 100;
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);
    hmc5883l_data_t d1;
    hmc5883l_read_gauss(&dev, &d1);
    float weak = d1.x_gauss;

    model.true_x = 800;
    hmc5883l_model_set_data(&model);
    hmc5883l_data_t d2;
    hmc5883l_read_gauss(&dev, &d2);
    float strong = d2.x_gauss;

    ASSERT_TRUE(strong > weak);
}

TEST_MAIN("hmc5883l hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_sets_config_a);
    RUN(test_init_sets_config_b);
    RUN(test_init_sets_continuous_mode);
    RUN(test_read_raw_values);
    RUN(test_read_gauss_reasonable);
    RUN(test_id_registers_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_config_b_writable_mask);
    RUN(test_idle_mode);
    RUN(test_different_field_strengths);
TEST_MAIN_END()
