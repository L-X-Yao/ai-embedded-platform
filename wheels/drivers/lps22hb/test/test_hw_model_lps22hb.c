/*
 * test_hw_model_lps22hb.c — tests using hardware model
 *
 * Validates LPS22HB driver against a datasheet-accurate model.
 * The model enforces register layout, POR defaults, BDU behavior,
 * one-shot conversion, ODR configuration, and data encoding.
 */

#include "test_harness.h"
#include "hw_model_lps22hb.h"
#include "../lps22hb.h"
#include <math.h>

static lps22hb_model_t model;

static const lps22hb_i2c_t model_i2c = {
    .write_reg = lps22hb_model_write_reg,
    .read_regs = lps22hb_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model,
};

static lps22hb_t dev;

static void setup(void)
{
    lps22hb_model_init(&model);
    lps22hb_init(&dev, &model_i2c);
}

/* --- init --- */

TEST(test_init_verifies_who_am_i)
{
    lps22hb_model_init(&model);
    lps22hb_t d;
    lps22hb_err_t err = lps22hb_init(&d, &model_i2c);
    ASSERT_EQ(err, LPS22HB_OK);
    ASSERT_TRUE(model.read_count >= 1);
}

TEST(test_init_sets_bdu)
{
    setup();
    /* After init, BDU (bit 1) in CTRL_REG1 must be set */
    ASSERT_TRUE(model.regs[LPS22HB_MODEL_REG_CTRL_REG1] & 0x02u);
}

TEST(test_init_odr_powerdown)
{
    setup();
    /* After init, ODR bits [6:4] must be 000 (power-down / one-shot) */
    uint8_t odr_bits = (model.regs[LPS22HB_MODEL_REG_CTRL_REG1] >> 4) & 0x07u;
    ASSERT_EQ(odr_bits, 0);
}

TEST(test_init_wrong_id_fails)
{
    lps22hb_model_init(&model);
    model.regs[LPS22HB_MODEL_REG_WHO_AM_I] = 0x00;  /* corrupt ID */
    lps22hb_t d;
    ASSERT_EQ(lps22hb_init(&d, &model_i2c), LPS22HB_ERR_ID);
}

/* --- pressure read round-trip --- */

TEST(test_read_standard_atmosphere)
{
    setup();
    /* 1013.25 hPa => raw = 1013.25 * 4096 = 4150272 */
    /* 25.00 C => raw = 2500 */
    lps22hb_model_inject(&model, 4150272, 2500);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 1013.25f, 0.01f);
    ASSERT_NEAR(data.temperature_c, 25.00f, 0.01f);
}

TEST(test_read_zero_values)
{
    setup();
    lps22hb_model_inject(&model, 0, 0);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 0.0f, 0.001f);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.001f);
}

TEST(test_read_negative_temperature)
{
    setup();
    /* -10.50 C => raw = -1050 */
    lps22hb_model_inject(&model, 4150272, -1050);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.temperature_c, -10.50f, 0.01f);
}

TEST(test_read_high_pressure)
{
    setup();
    /* 1260 hPa (sensor max) => raw = 1260 * 4096 = 5160960 = 0x4EC000 */
    lps22hb_model_inject(&model, 5160960, 0);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 1260.0f, 0.01f);
}

TEST(test_read_low_pressure)
{
    setup();
    /* 260 hPa (sensor min) => raw = 260 * 4096 = 1064960 */
    lps22hb_model_inject(&model, 1064960, 0);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 260.0f, 0.01f);
}

/* --- raw register read-back --- */

TEST(test_raw_pressure_roundtrip)
{
    setup();
    lps22hb_model_inject_pressure(&model, 4150272);

    int32_t raw;
    ASSERT_EQ(lps22hb_read_raw_pressure(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, 4150272);
}

TEST(test_raw_pressure_negative)
{
    setup();
    /* Negative 24-bit value: -4096 = 0xFFF000 */
    lps22hb_model_inject_pressure(&model, -4096);

    int32_t raw;
    ASSERT_EQ(lps22hb_read_raw_pressure(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, -4096);
}

TEST(test_raw_temperature_roundtrip)
{
    setup();
    lps22hb_model_inject_temperature(&model, 2500);

    int16_t raw;
    ASSERT_EQ(lps22hb_read_raw_temperature(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, 2500);
}

TEST(test_raw_temperature_negative)
{
    setup();
    lps22hb_model_inject_temperature(&model, -500);

    int16_t raw;
    ASSERT_EQ(lps22hb_read_raw_temperature(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, -500);
}

/* --- ODR configuration round-trip --- */

TEST(test_odr_all_values)
{
    setup();
    struct { lps22hb_odr_t odr; uint8_t expected_odr_bits; } cases[] = {
        { LPS22HB_ODR_ONE_SHOT, 0 },
        { LPS22HB_ODR_1HZ,     1 },
        { LPS22HB_ODR_10HZ,    2 },
        { LPS22HB_ODR_25HZ,    3 },
        { LPS22HB_ODR_50HZ,    4 },
        { LPS22HB_ODR_75HZ,    5 },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ASSERT_EQ(lps22hb_set_odr(&dev, cases[i].odr), LPS22HB_OK);
        uint8_t odr_bits =
            (model.regs[LPS22HB_MODEL_REG_CTRL_REG1] >> 4) & 0x07u;
        ASSERT_EQ(odr_bits, cases[i].expected_odr_bits);
        /* BDU must remain set after every ODR change */
        ASSERT_TRUE(model.regs[LPS22HB_MODEL_REG_CTRL_REG1] & 0x02u);
    }
}

TEST(test_odr_invalid_rejected)
{
    setup();
    ASSERT_EQ(lps22hb_set_odr(&dev, (lps22hb_odr_t)6), LPS22HB_ERR_PARAM);
    ASSERT_EQ(lps22hb_set_odr(&dev, (lps22hb_odr_t)255), LPS22HB_ERR_PARAM);
}

/* --- one-shot behavior --- */

TEST(test_one_shot_self_clears)
{
    setup();
    /* Ensure we are in one-shot mode (ODR=0 after init) */
    lps22hb_model_inject(&model, 4150272, 2500);

    ASSERT_EQ(lps22hb_one_shot_trigger(&dev), LPS22HB_OK);

    /* After model simulates conversion, ONE_SHOT bit (bit 0) self-clears */
    ASSERT_FALSE(model.regs[LPS22HB_MODEL_REG_CTRL_REG2] & 0x01u);
}

TEST(test_one_shot_sets_data_available)
{
    setup();
    lps22hb_model_inject(&model, 4150272, 2500);

    ASSERT_EQ(lps22hb_one_shot_trigger(&dev), LPS22HB_OK);

    /* STATUS should show both P_DA and T_DA */
    ASSERT_TRUE(model.regs[LPS22HB_MODEL_REG_STATUS] & 0x01u);
    ASSERT_TRUE(model.regs[LPS22HB_MODEL_REG_STATUS] & 0x02u);
}

TEST(test_one_shot_then_read)
{
    setup();
    /* Inject data, trigger one-shot, then read */
    lps22hb_model_inject(&model, 4150272, 2500);
    ASSERT_EQ(lps22hb_one_shot_trigger(&dev), LPS22HB_OK);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 1013.25f, 0.01f);
    ASSERT_NEAR(data.temperature_c, 25.00f, 0.01f);
}

/* --- BDU behavior --- */

TEST(test_bdu_clears_status_after_read)
{
    setup();
    /* BDU is set during init (bit 1 of CTRL_REG1) */
    ASSERT_TRUE(model.regs[LPS22HB_MODEL_REG_CTRL_REG1] & 0x02u);

    lps22hb_model_inject(&model, 4150272, 2500);
    ASSERT_TRUE(model.regs[LPS22HB_MODEL_REG_STATUS] & 0x03u);

    /* Reading data (PRESS_XL through TEMP_H) should clear DA flags */
    lps22hb_data_t data;
    lps22hb_read(&dev, &data);

    /* Both P_DA and T_DA should be cleared after a full read */
    ASSERT_FALSE(model.regs[LPS22HB_MODEL_REG_STATUS] & 0x01u);
    ASSERT_FALSE(model.regs[LPS22HB_MODEL_REG_STATUS] & 0x02u);
}

/* --- POR default verification --- */

TEST(test_por_who_am_i)
{
    lps22hb_model_t m;
    lps22hb_model_init(&m);
    ASSERT_EQ_HEX(m.regs[LPS22HB_MODEL_REG_WHO_AM_I], 0xB1);
}

TEST(test_por_ctrl_reg1_default)
{
    lps22hb_model_t m;
    lps22hb_model_init(&m);
    ASSERT_EQ_HEX(m.regs[LPS22HB_MODEL_REG_CTRL_REG1], 0x00);
}

TEST(test_por_ctrl_reg2_default)
{
    lps22hb_model_t m;
    lps22hb_model_init(&m);
    /* POR: IF_ADD_INC=1 (bit 4) => 0x10 */
    ASSERT_EQ_HEX(m.regs[LPS22HB_MODEL_REG_CTRL_REG2], 0x10);
}

TEST(test_por_status_default)
{
    lps22hb_model_t m;
    lps22hb_model_init(&m);
    ASSERT_EQ_HEX(m.regs[LPS22HB_MODEL_REG_STATUS], 0x00);
}

/* --- read-only register protection --- */

TEST(test_who_am_i_read_only)
{
    setup();
    /* Attempt to write to WHO_AM_I — model should silently ignore */
    lps22hb_model_write_reg(&model, 0x5C, LPS22HB_MODEL_REG_WHO_AM_I, 0x00);
    ASSERT_EQ_HEX(model.regs[LPS22HB_MODEL_REG_WHO_AM_I], 0xB1);
}

/* --- sequential inject-read cycles --- */

TEST(test_sequential_pressure_updates)
{
    setup();
    int32_t test_raws[] = { 0, 1064960, 4150272, 5160960 };
    float   expected[]  = { 0.0f, 260.0f, 1013.25f, 1260.0f };

    for (unsigned i = 0; i < sizeof(test_raws) / sizeof(test_raws[0]); i++) {
        lps22hb_model_inject(&model, test_raws[i], 0);
        lps22hb_data_t data;
        ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
        ASSERT_NEAR(data.pressure_hpa, expected[i], 0.01f);
    }
}

/* --- I2C operation counting --- */

TEST(test_init_io_count)
{
    lps22hb_model_init(&model);
    lps22hb_t d;
    lps22hb_init(&d, &model_i2c);
    /* init should perform: 1 read (WHO_AM_I) + 1 write (CTRL_REG1) */
    ASSERT_EQ(model.read_count, 1);
    ASSERT_EQ(model.write_count, 1);
}

TEST_MAIN("lps22hb hardware model")
    RUN(test_init_verifies_who_am_i);
    RUN(test_init_sets_bdu);
    RUN(test_init_odr_powerdown);
    RUN(test_init_wrong_id_fails);
    RUN(test_read_standard_atmosphere);
    RUN(test_read_zero_values);
    RUN(test_read_negative_temperature);
    RUN(test_read_high_pressure);
    RUN(test_read_low_pressure);
    RUN(test_raw_pressure_roundtrip);
    RUN(test_raw_pressure_negative);
    RUN(test_raw_temperature_roundtrip);
    RUN(test_raw_temperature_negative);
    RUN(test_odr_all_values);
    RUN(test_odr_invalid_rejected);
    RUN(test_one_shot_self_clears);
    RUN(test_one_shot_sets_data_available);
    RUN(test_one_shot_then_read);
    RUN(test_bdu_clears_status_after_read);
    RUN(test_por_who_am_i);
    RUN(test_por_ctrl_reg1_default);
    RUN(test_por_ctrl_reg2_default);
    RUN(test_por_status_default);
    RUN(test_who_am_i_read_only);
    RUN(test_sequential_pressure_updates);
    RUN(test_init_io_count);
TEST_MAIN_END()
