// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mlx90614.c — tests using hardware register model
 *
 * Validates MLX90614 driver against a SMBus command-response model.
 * The model simulates command byte write + 3-byte read (LSB, MSB, PEC)
 * for temperature and emissivity registers.
 */

#include "test_harness.h"
#include "hw_model_mlx90614.h"
#include "../mlx90614.h"
#include <math.h>

static mlx90614_model_t model;

static const mlx90614_i2c_t model_i2c = {
    .write    = mlx90614_model_write,
    .read     = mlx90614_model_read,
    .delay_ms = NULL,
    .ctx      = &model,
};

static const mlx90614_cfg_t default_cfg = {
    .addr = MLX90614_ADDR_DEFAULT,
};

static void setup(void)
{
    mlx90614_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    mlx90614_t dev;
    mlx90614_err_t err = mlx90614_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, MLX90614_OK);
}

TEST(test_init_null_param)
{
    setup();
    mlx90614_t dev;
    mlx90614_err_t err = mlx90614_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, MLX90614_ERR_PARAM);
}

/* --- temperature read tests ------------------------------------------- */

TEST(test_read_object_temp_25c)
{
    setup();
    mlx90614_model_set_object_temp(&model, 25.0f);
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    float temp;
    mlx90614_err_t err = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(err, MLX90614_OK);
    /* raw = (25 + 273.15) * 50 = 14907.5, back: 14908 * 0.02 - 273.15 = 25.01 */
    ASSERT_NEAR(temp, 25.0f, 0.5f);
}

TEST(test_read_ambient_temp_22c)
{
    setup();
    mlx90614_model_set_ambient_temp(&model, 22.0f);
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    float temp;
    mlx90614_err_t err = mlx90614_read_ambient_temp(&dev, &temp);
    ASSERT_EQ(err, MLX90614_OK);
    ASSERT_NEAR(temp, 22.0f, 0.5f);
}

TEST(test_read_object_temp_hot)
{
    setup();
    mlx90614_model_set_object_temp(&model, 200.0f);
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    float temp;
    mlx90614_read_object_temp(&dev, &temp);
    ASSERT_NEAR(temp, 200.0f, 0.5f);
}

TEST(test_read_object_temp_cold)
{
    setup();
    mlx90614_model_set_object_temp(&model, -10.0f);
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    float temp;
    mlx90614_read_object_temp(&dev, &temp);
    ASSERT_NEAR(temp, -10.0f, 0.5f);
}

/* --- varying temperature tests --------------------------------------- */

TEST(test_object_temp_increases_with_raw)
{
    setup();
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    mlx90614_model_set_object_temp(&model, 10.0f);
    float t1;
    mlx90614_read_object_temp(&dev, &t1);

    mlx90614_model_set_object_temp(&model, 80.0f);
    float t2;
    mlx90614_read_object_temp(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

/* --- emissivity tests ------------------------------------------------- */

TEST(test_read_emissivity_default)
{
    setup();
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    float em;
    mlx90614_err_t err = mlx90614_read_emissivity(&dev, &em);
    ASSERT_EQ(err, MLX90614_OK);
    ASSERT_NEAR(em, 1.0f, 0.01f);
}

TEST(test_set_emissivity)
{
    setup();
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    /* Need delay for EEPROM write */
    mlx90614_err_t err = mlx90614_set_emissivity(&dev, 0.95f);
    ASSERT_EQ(err, MLX90614_OK);

    /* Read back */
    float em;
    mlx90614_read_emissivity(&dev, &em);
    ASSERT_NEAR(em, 0.95f, 0.01f);
}

TEST(test_model_nacks_bad_pec)
{
    /* p18: "A NACK could be received after the PEC". CRC-8 of
     * B4 24 00 00 is 0x28, so the placeholder 0x00 must not get through. */
    static const uint8_t bad[]  = { 0x24, 0x00, 0x00, 0x00 };
    static const uint8_t good[] = { 0x24, 0x00, 0x00, 0x28 };
    setup();

    ASSERT_FALSE(mlx90614_model_write(&model, 0x5A, bad, 4));
    ASSERT_EQ_HEX(model.hw.regs[MLX_SLOT(MLX_REG_EMISSIVITY)], 0xFF);

    ASSERT_TRUE(mlx90614_model_write(&model, 0x5A, good, 4));
    ASSERT_EQ_HEX(model.hw.regs[MLX_SLOT(MLX_REG_EMISSIVITY)], 0x00);
}

TEST(test_model_read_matches_figure_6)
{
    /* Figure 6, p20: RAM 0x07 holding 0x3AD2 reads as D2 3A, PEC 0x30.
     * 0x3AD2 = 15058 = (28.01 + 273.15) * 50. */
    static const uint8_t cmd[] = { 0x07 };
    static const uint8_t expect[] = { 0xD2, 0x3A, 0x30 };
    uint8_t rx[3];
    setup();
    mlx90614_model_set_object_temp(&model, 28.01f);

    ASSERT_TRUE(mlx90614_model_write(&model, 0x5A, cmd, 1));
    ASSERT_TRUE(mlx90614_model_read(&model, 0x5A, rx, 3));
    ASSERT_MEMEQ_N(rx, expect, sizeof(expect));
}

TEST(test_set_emissivity_out_of_range)
{
    setup();
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    mlx90614_err_t err = mlx90614_set_emissivity(&dev, 0.05f);
    ASSERT_EQ(err, MLX90614_ERR_PARAM);
}

/* --- multiple reads --------------------------------------------------- */

TEST(test_multiple_reads_consistent)
{
    setup();
    mlx90614_model_set_object_temp(&model, 37.0f);
    mlx90614_t dev;
    mlx90614_init(&dev, &model_i2c, &default_cfg);

    float t1, t2;
    mlx90614_read_object_temp(&dev, &t1);
    mlx90614_read_object_temp(&dev, &t2);
    ASSERT_NEAR(t1, t2, 0.01f);
}

TEST_MAIN("mlx90614 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_read_object_temp_25c);
    RUN(test_read_ambient_temp_22c);
    RUN(test_read_object_temp_hot);
    RUN(test_read_object_temp_cold);
    RUN(test_object_temp_increases_with_raw);
    RUN(test_read_emissivity_default);
    RUN(test_set_emissivity);
    RUN(test_model_nacks_bad_pec);
    RUN(test_model_read_matches_figure_6);
    RUN(test_set_emissivity_out_of_range);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
