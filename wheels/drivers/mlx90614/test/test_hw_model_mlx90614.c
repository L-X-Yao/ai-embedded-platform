// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mlx90614.c — tests using hardware register model
 *
 * Validates MLX90614 driver against a SMBus command-response model.
 * The model simulates the SMBus read word (command byte, repeated START,
 * LSB, MSB, PEC) for temperature and emissivity registers.
 */

#include "test_harness.h"
#include "hw_model_mlx90614.h"
#include "../mlx90614.h"
#include <math.h>

static mlx90614_model_t model;

static const mlx90614_i2c_t model_i2c = {
    .write      = mlx90614_model_write,
    .delay_ms   = mlx90614_model_delay,
    .ctx        = &model,
    .write_read = mlx90614_model_write_read,
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

    ASSERT_TRUE(mlx90614_model_write_read(&model, 0x5A, cmd, 1, rx, 3));
    ASSERT_MEMEQ_N(rx, expect, sizeof(expect));
}

TEST(test_model_refuses_a_read_after_stop)
{
    /* p18: the read needs a repeated START, and a STOP ends the message */
    static const uint8_t cmd[] = { 0x07 };
    uint8_t rx[3];
    setup();

    ASSERT_TRUE(mlx90614_model_write(&model, 0x5A, cmd, 1));
    ASSERT_FALSE(mlx90614_model_read(&model, 0x5A, rx, 3));
    ASSERT_EQ(model.word_reads, 0);
}

TEST(test_read_object_temp_matches_figure_6)
{
    /* Figure 6, p20: the driver reads RAM 0x07 in one read word */
    setup();
    mlx90614_model_set_object_temp(&model, 28.01f);
    mlx90614_t dev;
    ASSERT_EQ(mlx90614_init(&dev, &model_i2c, &default_cfg), MLX90614_OK);
    model.word_reads = 0;

    float temp;
    ASSERT_EQ(mlx90614_read_object_temp(&dev, &temp), MLX90614_OK);
    ASSERT_EQ(model.word_reads, 1);
    ASSERT_EQ_HEX(model.last_cmd, 0x07);
    ASSERT_NEAR(temp, 28.01f, 0.011f);
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

/* --- EEPROM cells ------------------------------------------------------- */

/* Frames for cell 0x24 at address 0x5A, each with its PEC */
static bool write_cell(uint16_t val)
{
    uint8_t wire[4] = { 0xB4, 0x24, (uint8_t)(val & 0xFFu), (uint8_t)(val >> 8) };
    uint8_t frame[4] = { 0x24, wire[2], wire[3], mlx_model_pec(wire, 4) };
    return mlx90614_model_write(&model, 0x5A, frame, 4);
}

static uint16_t cell(void)
{
    uint8_t slot = MLX_SLOT(MLX_REG_EMISSIVITY);
    return (uint16_t)(model.hw.regs[slot] | (model.hw.regs[slot + 1] << 8));
}

/* p.13: "Erase (write 0) must take place before the write of the desired data
 * is done." p.19: "A write of 0x0000 must be done prior to writing in EEPROM" */
TEST(test_model_spoils_a_write_to_an_unerased_cell)
{
    setup();
    ASSERT_EQ_HEX(cell(), 0xFFFF);
    ASSERT_TRUE(write_cell(0xF332));
    ASSERT_TRUE(cell() != 0xF332);
    ASSERT_EQ(model.unerased_writes, 1);
}

/* p.16: "Wait at least 5ms" after the erase and after the write */
TEST(test_model_takes_a_write_after_erase_and_wait)
{
    setup();
    ASSERT_TRUE(write_cell(0x0000));
    ASSERT_EQ_HEX(cell(), 0x0000);
    mlx90614_model_delay(&model, 5);
    ASSERT_TRUE(write_cell(0xF332));
    ASSERT_EQ_HEX(cell(), 0xF332);
    ASSERT_EQ(model.unerased_writes, 0);
}

/* p.17: EEBUSY, "EEPROM is busy with executing the previous write/erase" */
TEST(test_model_loses_a_write_inside_the_wait)
{
    setup();
    ASSERT_TRUE(write_cell(0x0000));
    mlx90614_model_delay(&model, 4);
    ASSERT_TRUE(write_cell(0xF332));
    ASSERT_EQ_HEX(cell(), 0x0000);
    ASSERT_EQ(model.busy_writes, 1);
}

TEST(test_model_is_busy_after_a_write_as_well)
{
    setup();
    ASSERT_TRUE(write_cell(0x0000));
    mlx90614_model_delay(&model, 5);
    ASSERT_TRUE(write_cell(0xF332));
    ASSERT_TRUE(write_cell(0x0000));
    ASSERT_EQ_HEX(cell(), 0xF332);
    ASSERT_EQ(model.busy_writes, 1);
}

/* p.13: "Emissivity = dec2hex[ round( 65535 x e) ]", over the factory 0xFFFF */
TEST(test_set_emissivity_replaces_the_factory_value)
{
    setup();
    mlx90614_t dev;
    ASSERT_EQ(mlx90614_init(&dev, &model_i2c, &default_cfg), MLX90614_OK);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 0.95f), MLX90614_OK);
    ASSERT_EQ_HEX(cell(), 0xF332);
    ASSERT_EQ(model.unerased_writes, 0);
    ASSERT_EQ(model.busy_writes, 0);
}

/* Each call leaves the wait of p.16 behind it, so a second call works */
TEST(test_set_emissivity_twice)
{
    setup();
    mlx90614_t dev;
    ASSERT_EQ(mlx90614_init(&dev, &model_i2c, &default_cfg), MLX90614_OK);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 0.95f), MLX90614_OK);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 0.5f), MLX90614_OK);
    ASSERT_EQ_HEX(cell(), 0x8000);  /* round(32767.5) */
    ASSERT_EQ(model.busy_writes, 0);
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
    RUN(test_model_refuses_a_read_after_stop);
    RUN(test_read_object_temp_matches_figure_6);
    RUN(test_set_emissivity_out_of_range);
    RUN(test_multiple_reads_consistent);
    RUN(test_model_spoils_a_write_to_an_unerased_cell);
    RUN(test_model_takes_a_write_after_erase_and_wait);
    RUN(test_model_loses_a_write_inside_the_wait);
    RUN(test_model_is_busy_after_a_write_as_well);
    RUN(test_set_emissivity_replaces_the_factory_value);
    RUN(test_set_emissivity_twice);
TEST_MAIN_END()
