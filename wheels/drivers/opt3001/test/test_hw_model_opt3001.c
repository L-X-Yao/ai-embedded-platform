// SPDX-License-Identifier: MIT
/*
 * test_hw_model_opt3001.c — tests using hardware register model
 *
 * Validates OPT3001 driver against a datasheet-accurate register model.
 * The model enforces read-only manufacturer/device IDs, 16-bit register
 * protocol, lux encoding, config register mode bits, and CRF flag.
 */

#include "test_harness.h"
#include "hw_model_opt3001.h"
#include "../opt3001.h"
#include <math.h>

static opt3001_model_t model;

static const opt3001_i2c_t model_i2c = {
    .write_regs = opt3001_model_i2c_write_regs,
    .read_regs  = opt3001_model_i2c_read_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static void setup(void)
{
    opt3001_model_init(&model);
}

/* Register contents as the bus sees them, without the read side effects. */
static uint16_t cfg_raw(void)
{
    uint8_t base = OPT_BYTE_ADDR(OPT_REG_CONFIG);
    return (uint16_t)(((uint16_t)model.hw.regs[base] << 8) |
                      model.hw.regs[base + 1]);
}

static uint16_t result_raw(void)
{
    uint8_t base = OPT_BYTE_ADDR(OPT_REG_RESULT);
    return (uint16_t)(((uint16_t)model.hw.regs[base] << 8) |
                      model.hw.regs[base + 1]);
}

static void write_cfg(uint16_t val)
{
    uint8_t buf[2] = {(uint8_t)(val >> 8), (uint8_t)(val & 0xFFu)};
    ASSERT_TRUE(opt3001_model_i2c_write_regs(&model, OPT3001_ADDR_GND,
                                             OPT_REG_CONFIG, buf, 2));
}

/* A read over the bus, so unlike cfg_raw it has the side effect on CRF */
static void read_cfg(uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    *val = 0;
    ASSERT_TRUE(opt3001_model_i2c_read_regs(&model, OPT3001_ADDR_GND,
                                            OPT_REG_CONFIG, buf, 2));
    *val = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

/* SBOS681C Figure 24 / Table 10, p21-22 */
#define CFG_CT      (1u << 11)
#define CFG_M_MASK  (3u << 9)
#define CFG_M_SINGLE (1u << 9)
#define CFG_M_CONT10 (2u << 9)
#define CFG_M_CONT11 (3u << 9)
#define CFG_OVF     (1u << 8)
#define CFG_CRF     (1u << 7)
#define CFG_FH      (1u << 6)
#define CFG_FL      (1u << 5)

/* Total integration time max (110 / 880 ms, p6) plus the 3 ms of note 5, p5 */
#define CONV_MS_CT0 113u
#define CONV_MS_CT1 883u

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    ASSERT_EQ(err, OPT3001_OK);
}

TEST(test_init_wrong_manufacturer_id)
{
    setup();
    /* Corrupt manufacturer ID */
    uint8_t mfr_base = OPT_BYTE_ADDR(OPT_REG_MANUFACTURER_ID);
    model.hw.reset_values[mfr_base] = 0x00;
    model.hw.reset_values[mfr_base + 1] = 0x00;
    hw_model_reset(&model.hw);

    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    ASSERT_EQ(err, OPT3001_ERR_ID);
}

TEST(test_init_wrong_device_id)
{
    setup();
    /* Corrupt device ID */
    uint8_t dev_base = OPT_BYTE_ADDR(OPT_REG_DEVICE_ID);
    model.hw.reset_values[dev_base] = 0xFF;
    model.hw.reset_values[dev_base + 1] = 0xFF;
    hw_model_reset(&model.hw);

    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    ASSERT_EQ(err, OPT3001_ERR_ID);
}

TEST(test_init_writes_default_config)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    /* Default config 0xC010 should be written. The part resets to C810h, so
     * this fails when init writes nothing. */
    ASSERT_EQ_HEX(cfg_raw(), 0xC810u & ~CFG_CT);
    uint8_t cfg_base = OPT_BYTE_ADDR(OPT_REG_CONFIG);
    ASSERT_EQ(model.hw.regs[cfg_base], 0xC0u);
    ASSERT_EQ(model.hw.regs[cfg_base + 1], 0x10u);
}

TEST(test_init_invalid_address)
{
    setup();
    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, 0x30);
    ASSERT_EQ(err, OPT3001_ERR_PARAM);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_lux_reasonable)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);

    /* Trigger single-shot to populate result */
    opt3001_start_single(&dev);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);

    float lux = 0.0f;
    opt3001_err_t err = opt3001_read_lux(&dev, &lux);
    ASSERT_EQ(err, OPT3001_OK);
    /* With true_lux=500, encoding and decoding should be close */
    ASSERT_TRUE(lux > 400.0f && lux < 600.0f);
}

TEST(test_data_ready_after_single)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    opt3001_start_single(&dev);
    ASSERT_FALSE(opt3001_data_ready(&dev));
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_TRUE(opt3001_data_ready(&dev));
}

/* p14 note 4: "if two configuration register reads are performed immediately
 * after a conversion completion, the first reads 1 and the second reads 0." */
TEST(test_second_data_ready_is_false)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND);
    opt3001_start_single(&dev);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_TRUE(opt3001_data_ready(&dev));
    ASSERT_FALSE(opt3001_data_ready(&dev));
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_manufacturer_id_readonly)
{
    setup();
    uint8_t mfr_base = OPT_BYTE_ADDR(OPT_REG_MANUFACTURER_ID);
    hw_model__write_reg(&model.hw, mfr_base, 0xFF);
    ASSERT_EQ(model.hw.regs[mfr_base], 0x54u);
}

TEST(test_device_id_readonly)
{
    setup();
    uint8_t dev_base = OPT_BYTE_ADDR(OPT_REG_DEVICE_ID);
    hw_model__write_reg(&model.hw, dev_base, 0xFF);
    ASSERT_EQ(model.hw.regs[dev_base], 0x30u);
}

TEST(test_result_register_readonly)
{
    setup();
    uint8_t res_base = OPT_BYTE_ADDR(OPT_REG_RESULT);
    uint8_t orig = model.hw.regs[res_base];
    hw_model__write_reg(&model.hw, res_base, 0xFF);
    ASSERT_EQ(model.hw.regs[res_base], orig);
}

TEST(test_continuous_mode)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    opt3001_set_continuous(&dev);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);

    float lux = 0.0f;
    opt3001_err_t err = opt3001_read_lux(&dev, &lux);
    ASSERT_EQ(err, OPT3001_OK);
    ASSERT_TRUE(lux > 0.0f);
}

TEST(test_different_light_levels)
{
    setup();
    model.true_lux = 100.0f;
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    opt3001_start_single(&dev);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    float low_lux = 0.0f;
    opt3001_read_lux(&dev, &low_lux);

    model.true_lux = 10000.0f;
    opt3001_start_single(&dev);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    float high_lux = 0.0f;
    opt3001_read_lux(&dev, &high_lux);

    ASSERT_TRUE(high_lux > low_lux);
}

/* --- what the model itself must do (SBOS681C) ---------------------------- */

/* p21: "Configuration Register (offset = 01h) [reset = C810h]" */
TEST(test_power_on_config_is_c810)
{
    setup();
    ASSERT_EQ_HEX(cfg_raw(), 0xC810u);
}

/* p22: OVF, CRF, FH and FL are R in Table 10, so a write cannot set them */
TEST(test_status_fields_cannot_be_set)
{
    setup();
    write_cfg(0xFFFFu & (uint16_t)~CFG_M_MASK);
    ASSERT_EQ_HEX(cfg_raw() & (CFG_OVF | CFG_CRF | CFG_FH | CFG_FL), 0u);
    /* The R/W fields of the same word did take the write */
    ASSERT_EQ_HEX(cfg_raw() & 0xF81Fu, 0xF81Fu);
}

/* ... and a write of 0 cannot clear them. M = 00b keeps CRF: "Writing a
 * shutdown mode does not affect the state of this field", p22. */
TEST(test_status_fields_cannot_be_cleared)
{
    setup();
    model.true_lux = 100000.0f;
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_EQ_HEX(cfg_raw() & (CFG_OVF | CFG_CRF), CFG_OVF | CFG_CRF);
    write_cfg(0x0000u);
    ASSERT_EQ_HEX(cfg_raw() & (CFG_OVF | CFG_CRF), CFG_OVF | CFG_CRF);
}

/* p22: CRF "is cleared (set to 0) when the configuration register is
 * subsequently read or written with any value except one containing the
 * shutdown mode" */
TEST(test_crf_cleared_by_read)
{
    setup();
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    uint16_t cfg;
    read_cfg(&cfg);
    ASSERT_EQ_HEX(cfg & CFG_CRF, CFG_CRF);
    read_cfg(&cfg);
    ASSERT_EQ_HEX(cfg & CFG_CRF, 0u);
}

TEST(test_crf_cleared_by_non_shutdown_write)
{
    setup();
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, CFG_CRF);
    write_cfg(0xC010u | CFG_M_CONT11);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, 0u);
}

/* p22: M "continues to read 01b while the device is actively converting.
 * When the single-shot conversion is complete, the mode of conversion
 * operation field is automatically set to 00b". 113 ms is the 110 ms maximum
 * (p6) plus "the integration time plus 3 ms" (p5). */
TEST(test_single_shot_runs_for_the_conversion_time)
{
    setup();
    model.true_lux = 500.0f;
    write_cfg(0xC010u | CFG_M_SINGLE);
    ASSERT_EQ_HEX(cfg_raw() & CFG_M_MASK, CFG_M_SINGLE);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, 0u);
    ASSERT_EQ_HEX(result_raw(), 0u);

    opt3001_model_advance_ms(&model, CONV_MS_CT0 - 1u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_M_MASK, CFG_M_SINGLE);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, 0u);
    ASSERT_EQ_HEX(result_raw(), 0u);

    opt3001_model_advance_ms(&model, 1u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_M_MASK, 0u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, CFG_CRF);
    ASSERT_TRUE(result_raw() != 0u);
}

/* p22: CT, "1 = 800 ms" */
TEST(test_ct_set_takes_800_ms)
{
    setup();
    write_cfg(0xC810u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT1 - 1u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, 0u);
    opt3001_model_advance_ms(&model, 1u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, CFG_CRF);
}

/* p22: "10, 11 = Continuous conversions". M stays and every period ends with
 * a new result. */
static void check_continuous(uint16_t mode)
{
    uint16_t cfg;
    setup();
    model.true_lux = 100.0f;
    write_cfg((uint16_t)(0xC010u | mode));
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_EQ_HEX(cfg_raw() & CFG_M_MASK, mode);
    read_cfg(&cfg);
    ASSERT_EQ_HEX(cfg & CFG_CRF, CFG_CRF);
    uint16_t first = result_raw();
    ASSERT_TRUE(first != 0u);

    model.true_lux = 10000.0f;
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_EQ_HEX(cfg_raw() & CFG_M_MASK, mode);
    read_cfg(&cfg);
    ASSERT_EQ_HEX(cfg & CFG_CRF, CFG_CRF);
    ASSERT_TRUE(result_raw() != first);
}

TEST(test_mode_10b_is_continuous)
{
    check_continuous(CFG_M_CONT10);
}

TEST(test_mode_11b_is_continuous)
{
    check_continuous(CFG_M_CONT11);
}

/* p21: "If a measurement conversion is in progress when the configuration
 * register is written, the active measurement conversion immediately aborts." */
TEST(test_config_write_aborts_conversion)
{
    setup();
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, 50u);
    write_cfg(0xC010u);
    opt3001_model_advance_ms(&model, 1000u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, 0u);
    ASSERT_EQ_HEX(result_raw(), 0u);
}

/* p21: "If the new configuration register directs a new conversion, that
 * conversion is subsequently started." */
TEST(test_config_write_restarts_conversion)
{
    setup();
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, 50u);
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT0 - 1u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, 0u);
    opt3001_model_advance_ms(&model, 1u);
    ASSERT_EQ_HEX(cfg_raw() & CFG_CRF, CFG_CRF);
}

/* Lux in, lux out through the Equation 3 of p20: lux = 0.01 * 2^E * R */
static float decoded(uint16_t raw)
{
    return 0.01f * (float)(1u << (raw >> 12)) * (float)(raw & 0x0FFFu);
}

/* Table 9, p21. Each row must come back to within one LSB of its range. */
TEST(test_encode_table_9_values)
{
    static const struct { float lux; float lsb; } rows[] = {
        {    0.01f,  0.01f }, {   40.95f,  0.01f }, {   88.80f,  0.08f },
        { 2818.56f,  1.28f }, { 5242.88f,  2.56f }, {   20.48f,  0.01f },
        { 83865.60f, 20.48f },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ASSERT_NEAR(decoded(opt3001_encode_lux(rows[i].lux)), rows[i].lux,
                    rows[i].lsb);
    }
}

/* Table 9, p21: the last row is E = 0Bh, R = FFFh, 83865.60 lux */
TEST(test_encode_full_scale_is_bfff)
{
    ASSERT_EQ_HEX(opt3001_encode_lux(83865.6f), 0xBFFFu);
}

/* Table 9, p21: range 0Bh takes any R, so 50000 lux is not clamped */
TEST(test_encode_top_range_keeps_mantissa)
{
    uint16_t raw = opt3001_encode_lux(50000.0f);
    ASSERT_EQ_HEX(raw >> 12, 0x0Bu);
    ASSERT_NEAR(decoded(raw), 50000.0f, 20.48f);
}

/* 660 / 0.01 is above 65535, so a cast to uint16_t before the range test
 * wrapped and gave E = 0. */
TEST(test_encode_quotient_above_16_bits)
{
    ASSERT_NEAR(decoded(opt3001_encode_lux(660.0f)), 660.0f, 0.32f);
}

/* p22: with auto range, OVF is set "if the input light is beyond the
 * full-scale level of the entire device", and "The field is reevaluated on
 * every measurement." */
TEST(test_ovf_follows_each_measurement)
{
    setup();
    model.true_lux = 100000.0f;
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_EQ_HEX(cfg_raw() & CFG_OVF, CFG_OVF);
    ASSERT_EQ_HEX(result_raw(), 0xBFFFu);

    model.true_lux = 100.0f;
    write_cfg(0xC010u | CFG_M_SINGLE);
    opt3001_model_advance_ms(&model, CONV_MS_CT0);
    ASSERT_EQ_HEX(cfg_raw() & CFG_OVF, 0u);
}

/* p16: "The next two bytes are written to the register", and a read gives
 * the MSB then the LSB of that one register. */
TEST(test_register_access_is_two_bytes)
{
    setup();
    uint8_t buf[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    ASSERT_FALSE(opt3001_model_i2c_write_regs(&model, OPT3001_ADDR_GND,
                                              OPT_REG_CONFIG, buf, 1));
    ASSERT_EQ_HEX(cfg_raw(), 0xC810u);
    ASSERT_FALSE(opt3001_model_i2c_read_regs(&model, OPT3001_ADDR_GND,
                                             OPT_REG_CONFIG, buf, 4));
}

TEST_MAIN("opt3001 hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_manufacturer_id);
    RUN(test_init_wrong_device_id);
    RUN(test_init_writes_default_config);
    RUN(test_init_invalid_address);
    RUN(test_read_lux_reasonable);
    RUN(test_data_ready_after_single);
    RUN(test_second_data_ready_is_false);
    RUN(test_manufacturer_id_readonly);
    RUN(test_device_id_readonly);
    RUN(test_result_register_readonly);
    RUN(test_continuous_mode);
    RUN(test_different_light_levels);
    RUN(test_power_on_config_is_c810);
    RUN(test_status_fields_cannot_be_set);
    RUN(test_status_fields_cannot_be_cleared);
    RUN(test_crf_cleared_by_read);
    RUN(test_crf_cleared_by_non_shutdown_write);
    RUN(test_single_shot_runs_for_the_conversion_time);
    RUN(test_ct_set_takes_800_ms);
    RUN(test_mode_10b_is_continuous);
    RUN(test_mode_11b_is_continuous);
    RUN(test_config_write_aborts_conversion);
    RUN(test_config_write_restarts_conversion);
    RUN(test_encode_table_9_values);
    RUN(test_encode_full_scale_is_bfff);
    RUN(test_encode_top_range_keeps_mantissa);
    RUN(test_encode_quotient_above_16_bits);
    RUN(test_ovf_follows_each_measurement);
    RUN(test_register_access_is_two_bytes);
TEST_MAIN_END()
