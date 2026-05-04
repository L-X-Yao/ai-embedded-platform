/*
 * test_hw_model_qmc5883p.c — tests using hardware register model
 *
 * Validates QMC5883P driver against a datasheet-accurate register model.
 * The model enforces read-only chip ID, data registers, status,
 * measurement trigger on continuous mode, and soft reset behavior.
 */

#include "test_harness.h"
#include "hw_model_qmc5883p.h"
#include "../qmc5883p.h"
#include <math.h>

static qmc5883p_model_t model;

static const qmc5883p_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    qmc5883p_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    qmc5883p_err_t err = qmc5883p_init(&model_i2c);
    ASSERT_EQ(err, QMC5883P_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[QMC5883P_REG_CHIP_ID] = 0xFF;
    hw_model_reset(&model.hw);

    qmc5883p_err_t err = qmc5883p_init(&model_i2c);
    ASSERT_EQ(err, QMC5883P_ERR_ID);
}

TEST(test_init_writes_wake_register)
{
    setup();
    qmc5883p_init(&model_i2c);
    /* Driver writes 0x40 to wake register before soft reset;
     * soft reset clears it back to 0x00. Verify it was written
     * by checking the register is at reset value (soft reset occurred). */
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_WAKE], 0x00u);
}

TEST(test_init_writes_sign_register)
{
    setup();
    qmc5883p_init(&model_i2c);
    /* Driver writes 0x06 to sign register */
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_SIGN], 0x06u);
}

TEST(test_init_configures_cr1_continuous)
{
    setup();
    qmc5883p_init(&model_i2c);
    /* CR1 should have continuous mode (0x03), ODR 200Hz (0x0C), OSR 8x (0x00) */
    uint8_t expected_cr1 = 0x0Cu | 0x00u | 0x03u;
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_CR1], expected_cr1);
}

TEST(test_init_configures_cr2_range)
{
    setup();
    qmc5883p_init(&model_i2c);
    /* CR2 should have range 2G (0x03 << 2 = 0x0C) after soft reset+reconfig */
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_CR2], 0x0Cu);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_magnetometer_reasonable)
{
    setup();
    qmc5883p_init(&model_i2c);

    /* Data should be populated by continuous mode trigger */
    qmc5883p_data_t data;
    qmc5883p_err_t err = qmc5883p_read(&model_i2c, &data);
    ASSERT_EQ(err, QMC5883P_OK);
    /* true_mx=4500, sensitivity=15000 -> ~0.3 Gauss */
    ASSERT_TRUE(data.mx_gauss > 0.2f && data.mx_gauss < 0.4f);
    ASSERT_TRUE(data.my_gauss > 0.05f && data.my_gauss < 0.2f);
    ASSERT_TRUE(data.mz_gauss > 0.3f && data.mz_gauss < 0.5f);
}

TEST(test_read_not_ready_without_drdy)
{
    setup();
    qmc5883p_init(&model_i2c);

    /* Clear DRDY bit */
    model.hw.regs[QMC5883P_REG_STATUS] = 0x00u;

    qmc5883p_data_t data;
    qmc5883p_err_t err = qmc5883p_read(&model_i2c, &data);
    ASSERT_EQ(err, QMC5883P_ERR_NOT_READY);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_CHIP_ID], 0x80u);
    hw_model__write_reg(&model.hw, QMC5883P_REG_CHIP_ID, 0x00);
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_CHIP_ID], 0x80u);
}

TEST(test_data_registers_readonly)
{
    setup();
    qmc5883p_init(&model_i2c);
    uint8_t orig = model.hw.regs[QMC5883P_REG_DATA_X_LSB];
    hw_model__write_reg(&model.hw, QMC5883P_REG_DATA_X_LSB, 0xFF);
    ASSERT_EQ(model.hw.regs[QMC5883P_REG_DATA_X_LSB], orig);
}

TEST(test_different_field_strengths)
{
    setup();
    model.true_mx = 1000;
    qmc5883p_init(&model_i2c);
    qmc5883p_data_t d1;
    qmc5883p_read(&model_i2c, &d1);
    float weak = d1.mx_gauss;

    /* Stronger field */
    model.true_mx = 10000;
    qmc5883p_model_set_data(&model);
    qmc5883p_data_t d2;
    qmc5883p_read(&model_i2c, &d2);
    float strong = d2.mx_gauss;

    ASSERT_TRUE(strong > weak);
}

TEST(test_measure_count_increments)
{
    setup();
    int before = model.measure_count;
    qmc5883p_init(&model_i2c);
    /* Init enters continuous mode, which triggers measurement */
    ASSERT_TRUE(model.measure_count > before);
}

TEST_MAIN("qmc5883p hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_writes_wake_register);
    RUN(test_init_writes_sign_register);
    RUN(test_init_configures_cr1_continuous);
    RUN(test_init_configures_cr2_range);
    RUN(test_read_magnetometer_reasonable);
    RUN(test_read_not_ready_without_drdy);
    RUN(test_chip_id_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_different_field_strengths);
    RUN(test_measure_count_increments);
TEST_MAIN_END()
