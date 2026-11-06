// SPDX-License-Identifier: MIT
/*
 * test_hw_model_sgp30.c -- tests using hardware model
 *
 * Validates SGP30 driver against a command-protocol-accurate model.
 * The model tracks commands, returns CRC-protected responses, and
 * maintains internal state (TVOC, eCO2, baseline, serial, humidity).
 */

#include "test_harness.h"
#include "hw_model_sgp30.h"
#include "../sgp30.h"

static sgp30_model_t model;

static const sgp30_i2c_t model_i2c = {
    .write    = sgp30_model_write,
    .read     = sgp30_model_read,
    .delay_ms = sgp30_model_delay,
    .ctx      = &model,
};

static sgp30_t dev;

static void setup(void)
{
    sgp30_model_init(&model);
    sgp30_init(&dev, &model_i2c);
}

/* --- init --- */

TEST(test_init_sends_iaq_init)
{
    sgp30_model_init(&model);
    sgp30_t d;
    sgp30_err_t e = sgp30_init(&d, &model_i2c);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.last_cmd, SGP30_CMD_IAQ_INIT);
    ASSERT_TRUE(model.iaq_initialized);
    ASSERT_EQ(model.init_count, 1);
}

/* --- measure_iaq --- */

TEST(test_measure_iaq_default_clean_air)
{
    setup();
    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_OK);
    /* Model defaults: eco2=400, tvoc=0 */
    ASSERT_EQ(data.eco2_ppm, 400);
    ASSERT_EQ(data.tvoc_ppb, 0);
}

TEST(test_measure_iaq_model_values)
{
    setup();
    sgp30_model_set_iaq(&model, 1200, 350);

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(data.eco2_ppm, 1200);
    ASSERT_EQ(data.tvoc_ppb, 350);
}

TEST(test_measure_iaq_max_values)
{
    setup();
    sgp30_model_set_iaq(&model, 60000, 60000);

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(data.eco2_ppm, 60000);
    ASSERT_EQ(data.tvoc_ppb, 60000);
}

TEST(test_measure_iaq_increments_counter)
{
    setup();
    sgp30_data_t data;
    sgp30_measure_iaq(&dev, &data);
    sgp30_measure_iaq(&dev, &data);
    sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(model.measure_count, 3);
}

TEST(test_measure_iaq_sends_correct_cmd)
{
    setup();
    sgp30_data_t data;
    sgp30_measure_iaq(&dev, &data);
    /* last_cmd is overwritten by read, but write sets it to 0x2008 */
    /* The model tracks the command -- verify via measure_count */
    ASSERT_EQ(model.measure_count, 1);
}

/* --- get_baseline --- */

TEST(test_get_baseline_default)
{
    setup();
    sgp30_baseline_t bl;
    sgp30_err_t e = sgp30_get_baseline(&dev, &bl);
    ASSERT_EQ(e, SGP30_OK);
    /* Model defaults: eco2_bl=0x5678, tvoc_bl=0x1234 */
    ASSERT_EQ_HEX(bl.eco2_baseline, 0x5678);
    ASSERT_EQ_HEX(bl.tvoc_baseline, 0x1234);
}

TEST(test_get_baseline_custom)
{
    setup();
    sgp30_model_set_baseline(&model, 0xABCD, 0xEF01);

    sgp30_baseline_t bl;
    sgp30_err_t e = sgp30_get_baseline(&dev, &bl);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(bl.eco2_baseline, 0xABCD);
    ASSERT_EQ_HEX(bl.tvoc_baseline, 0xEF01);
}

/* --- set_baseline --- */

TEST(test_set_baseline_round_trip)
{
    setup();
    sgp30_baseline_t bl_in = { .tvoc_baseline = 0x1111, .eco2_baseline = 0x2222 };
    sgp30_err_t e = sgp30_set_baseline(&dev, &bl_in);
    ASSERT_EQ(e, SGP30_OK);

    /* Verify model received the values */
    ASSERT_EQ_HEX(model.tvoc_baseline, 0x1111);
    ASSERT_EQ_HEX(model.eco2_baseline, 0x2222);

    /* Read back through get_baseline */
    sgp30_baseline_t bl_out;
    e = sgp30_get_baseline(&dev, &bl_out);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(bl_out.tvoc_baseline, 0x1111);
    ASSERT_EQ_HEX(bl_out.eco2_baseline, 0x2222);
}

TEST(test_set_baseline_zero)
{
    setup();
    sgp30_baseline_t bl = { .tvoc_baseline = 0, .eco2_baseline = 0 };
    sgp30_err_t e = sgp30_set_baseline(&dev, &bl);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.tvoc_baseline, 0x0000);
    ASSERT_EQ_HEX(model.eco2_baseline, 0x0000);
}

TEST(test_set_baseline_max)
{
    setup();
    sgp30_baseline_t bl = { .tvoc_baseline = 0xFFFF, .eco2_baseline = 0xFFFF };
    sgp30_err_t e = sgp30_set_baseline(&dev, &bl);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.tvoc_baseline, 0xFFFF);
    ASSERT_EQ_HEX(model.eco2_baseline, 0xFFFF);
}

/* --- get_serial_id --- */

TEST(test_get_serial_default)
{
    setup();
    sgp30_serial_t serial;
    sgp30_err_t e = sgp30_get_serial_id(&dev, &serial);
    ASSERT_EQ(e, SGP30_OK);
    /* Model defaults: 0x0000, 0x0140, 0x7FCA */
    ASSERT_EQ(serial.id[0], 0x00);
    ASSERT_EQ(serial.id[1], 0x00);
    ASSERT_EQ(serial.id[2], 0x01);
    ASSERT_EQ(serial.id[3], 0x40);
    ASSERT_EQ(serial.id[4], 0x7F);
    ASSERT_EQ(serial.id[5], 0xCA);
}

TEST(test_get_serial_custom)
{
    setup();
    sgp30_model_set_serial(&model, 0xDEAD, 0xBEEF, 0xCAFE);

    sgp30_serial_t serial;
    sgp30_err_t e = sgp30_get_serial_id(&dev, &serial);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(serial.id[0], 0xDE);
    ASSERT_EQ_HEX(serial.id[1], 0xAD);
    ASSERT_EQ_HEX(serial.id[2], 0xBE);
    ASSERT_EQ_HEX(serial.id[3], 0xEF);
    ASSERT_EQ_HEX(serial.id[4], 0xCA);
    ASSERT_EQ_HEX(serial.id[5], 0xFE);
}

/* --- set_humidity --- */

TEST(test_set_humidity_value)
{
    setup();
    /* 15.5 g/m3 = 0x0F80 */
    sgp30_err_t e = sgp30_set_humidity(&dev, 0x0F80);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.abs_humidity, 0x0F80);
}

TEST(test_set_humidity_disable)
{
    setup();
    /* First set a value, then disable */
    sgp30_set_humidity(&dev, 0x0F80);
    ASSERT_EQ_HEX(model.abs_humidity, 0x0F80);

    sgp30_err_t e = sgp30_set_humidity(&dev, 0x0000);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.abs_humidity, 0x0000);
}

TEST(test_set_humidity_max)
{
    setup();
    sgp30_err_t e = sgp30_set_humidity(&dev, 0xFFFF);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.abs_humidity, 0xFFFF);
}

/* --- CRC integrity (model vs driver agree) --- */

TEST(test_model_crc_matches_driver_crc)
{
    /* Both model and driver implement the same Sensirion CRC-8 */
    uint8_t vec1[] = { 0xBE, 0xEF };
    ASSERT_EQ(sgp30_model_crc8(vec1, 2), sgp30_crc8(vec1, 2));

    uint8_t vec2[] = { 0x00, 0x00 };
    ASSERT_EQ(sgp30_model_crc8(vec2, 2), sgp30_crc8(vec2, 2));

    uint8_t vec3[] = { 0xFF, 0xFF };
    ASSERT_EQ(sgp30_model_crc8(vec3, 2), sgp30_crc8(vec3, 2));

    uint8_t vec4[] = { 0x01, 0x90 };
    ASSERT_EQ(sgp30_model_crc8(vec4, 2), sgp30_crc8(vec4, 2));
}

/* --- full workflow --- */

TEST(test_full_workflow)
{
    /* init -> set_humidity -> measure -> get_baseline -> set_baseline -> get_serial */
    sgp30_model_init(&model);
    sgp30_t d;

    sgp30_err_t e = sgp30_init(&d, &model_i2c);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_TRUE(model.iaq_initialized);

    /* Set humidity compensation */
    e = sgp30_set_humidity(&d, 0x0C80);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(model.abs_humidity, 0x0C80);

    /* Simulate polluted air */
    sgp30_model_set_iaq(&model, 2500, 800);

    sgp30_data_t data;
    e = sgp30_measure_iaq(&d, &data);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(data.eco2_ppm, 2500);
    ASSERT_EQ(data.tvoc_ppb, 800);

    /* Save baseline */
    sgp30_baseline_t bl;
    e = sgp30_get_baseline(&d, &bl);
    ASSERT_EQ(e, SGP30_OK);

    /* Restore baseline (simulates power cycle) */
    sgp30_model_init(&model);
    sgp30_init(&d, &model_i2c);
    e = sgp30_set_baseline(&d, &bl);
    ASSERT_EQ(e, SGP30_OK);

    /* Verify restored values */
    sgp30_baseline_t bl2;
    e = sgp30_get_baseline(&d, &bl2);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(bl2.eco2_baseline, bl.eco2_baseline);
    ASSERT_EQ_HEX(bl2.tvoc_baseline, bl.tvoc_baseline);

    /* Read serial */
    sgp30_serial_t serial;
    e = sgp30_get_serial_id(&d, &serial);
    ASSERT_EQ(e, SGP30_OK);
}

/* --- state changes between measurements --- */

TEST(test_iaq_changes_between_reads)
{
    setup();
    sgp30_data_t data;

    /* First measurement: clean air */
    sgp30_model_set_iaq(&model, 400, 0);
    sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(data.eco2_ppm, 400);
    ASSERT_EQ(data.tvoc_ppb, 0);

    /* Second measurement: polluted */
    sgp30_model_set_iaq(&model, 5000, 1200);
    sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(data.eco2_ppm, 5000);
    ASSERT_EQ(data.tvoc_ppb, 1200);

    /* Third measurement: back to clean */
    sgp30_model_set_iaq(&model, 400, 0);
    sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(data.eco2_ppm, 400);
    ASSERT_EQ(data.tvoc_ppb, 0);
}

/* --- the model, from the manual ---------------------------------------- */

/* A command as the master sends it, past any wait left from setup */
static bool send(uint16_t cmd)
{
    uint8_t tx[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFFu) };
    return sgp30_model_write(&model, 0x58, tx, 2);
}

/* Table 10, p.11: the maximum duration of each command without parameters.
 * p.8: "When the measurement is in progress, no communication with the sensor
 * is possible and the sensor aborts the communication with a XCK condition." */
TEST(test_no_communication_for_the_maximum_duration)
{
    static const struct { uint16_t cmd; uint16_t max_ms; } table10[] = {
        {0x2003, 10}, {0x2008, 12}, {0x2015, 10},
    };
    for (size_t i = 0; i < 3; i++) {
        uint8_t buf[6];
        sgp30_model_init(&model);
        ASSERT_TRUE(send(table10[i].cmd));
        ASSERT_FALSE(sgp30_model_read(&model, 0x58, buf, 6));
        ASSERT_FALSE(send(0x2008));
        sgp30_model_delay(&model, (uint16_t)(table10[i].max_ms - 1));
        ASSERT_FALSE(sgp30_model_read(&model, 0x58, buf, 6));
        sgp30_model_delay(&model, 1);
        ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 6));
    }
}

/* Table 10: set_iaq_baseline and set_absolute_humidity take 10 ms as well */
TEST(test_set_commands_are_busy_too)
{
    uint8_t humidity[5] = { 0x20, 0x61, 0x0C, 0x80, 0x00 };
    uint8_t baseline[8] = { 0x20, 0x1E, 0x11, 0x11, 0x00, 0x22, 0x22, 0x00 };
    humidity[4] = sgp30_model_crc8(&humidity[2], 2);
    baseline[4] = sgp30_model_crc8(&baseline[2], 2);
    baseline[7] = sgp30_model_crc8(&baseline[5], 2);

    sgp30_model_init(&model);
    ASSERT_TRUE(sgp30_model_write(&model, 0x58, humidity, 5));
    sgp30_model_delay(&model, 9);
    ASSERT_FALSE(send(0x2008));
    sgp30_model_delay(&model, 1);
    ASSERT_TRUE(sgp30_model_write(&model, 0x58, baseline, 8));
    sgp30_model_delay(&model, 9);
    ASSERT_FALSE(send(0x2008));
    sgp30_model_delay(&model, 1);
    ASSERT_TRUE(send(0x2008));
}

/* p.12: "the sensor needs the time tIDLE = 0.5ms to respond to the I2C read
 * header with an ACK Bit" */
TEST(test_serial_id_needs_0_5_ms)
{
    uint8_t buf[9];
    sgp30_model_init(&model);
    ASSERT_TRUE(send(0x3682));
    ASSERT_FALSE(sgp30_model_read(&model, 0x58, buf, 9));
    sgp30_model_delay(&model, 1);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 9));
}

/* p.8: "Note that the data cannot be read more than once, and access to data
 * beyond the specified amount will return a pattern of 1s." */
TEST(test_data_reads_once)
{
    uint8_t buf[6];
    sgp30_model_init(&model);
    ASSERT_TRUE(send(0x2008));
    sgp30_model_delay(&model, 12);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 6));
    ASSERT_EQ_HEX(buf[0], 400 >> 8);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 6));
    for (int i = 0; i < 6; i++) ASSERT_EQ_HEX(buf[i], 0xFF);
}

TEST(test_bytes_past_the_response_are_ones)
{
    uint8_t buf[9];
    sgp30_model_init(&model);
    ASSERT_TRUE(send(0x2008));
    sgp30_model_delay(&model, 12);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 9));
    ASSERT_EQ_HEX(buf[0], 400 >> 8);
    for (int i = 6; i < 9; i++) ASSERT_EQ_HEX(buf[i], 0xFF);
}

/* p.8: the master "can abort the read transfer ... after any data byte", and
 * what it left unread is gone as well */
TEST(test_a_cut_read_ends_the_data)
{
    uint8_t buf[6];
    sgp30_model_init(&model);
    ASSERT_TRUE(send(0x2008));
    sgp30_model_delay(&model, 12);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 2));
    ASSERT_EQ_HEX(buf[0], 400 >> 8);
    ASSERT_EQ_HEX(buf[1], 400 & 0xFF);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 2));
    ASSERT_EQ_HEX(buf[0], 0xFF);
}

/* Table 10: iaq_init has no response, so there is nothing but 1s to read */
TEST(test_a_command_with_no_response_reads_ones)
{
    uint8_t buf[3];
    sgp30_model_init(&model);
    ASSERT_TRUE(send(0x2003));
    sgp30_model_delay(&model, 10);
    ASSERT_TRUE(sgp30_model_read(&model, 0x58, buf, 3));
    ASSERT_EQ_HEX(buf[0], 0xFF);
    ASSERT_EQ_HEX(buf[2], 0xFF);
}

/* p.8: "the SGP30 only accepts data if it is followed by the correct checksum" */
TEST(test_data_needs_its_checksum)
{
    uint8_t humidity[5] = { 0x20, 0x61, 0x0C, 0x80, 0x00 };
    humidity[4] = (uint8_t)~sgp30_model_crc8(&humidity[2], 2);
    sgp30_model_init(&model);
    ASSERT_TRUE(sgp30_model_write(&model, 0x58, humidity, 5));
    ASSERT_EQ_HEX(model.abs_humidity, 0x0000);
}

/* Table 10: the parameter of set_absolute_humidity is 3 bytes, no fewer */
TEST(test_a_short_parameter_is_not_taken)
{
    uint8_t humidity[4] = { 0x20, 0x61, 0x0C, 0x80 };
    sgp30_model_init(&model);
    ASSERT_TRUE(sgp30_model_write(&model, 0x58, humidity, 4));
    ASSERT_EQ_HEX(model.abs_humidity, 0x0000);
}

/* --- the driver on it --------------------------------------------------- */

/* p.8: the driver waits out every command, so nothing is ever aborted */
TEST(test_driver_never_talks_to_a_busy_sensor)
{
    sgp30_data_t data;
    sgp30_baseline_t bl;
    sgp30_serial_t serial;

    setup();
    ASSERT_EQ(sgp30_set_humidity(&dev, 0x0C80), SGP30_OK);
    ASSERT_EQ(sgp30_measure_iaq(&dev, &data), SGP30_OK);
    ASSERT_EQ(sgp30_get_baseline(&dev, &bl), SGP30_OK);
    ASSERT_EQ(sgp30_set_baseline(&dev, &bl), SGP30_OK);
    ASSERT_EQ(sgp30_get_serial_id(&dev, &serial), SGP30_OK);
    ASSERT_EQ(sgp30_measure_iaq(&dev, &data), SGP30_OK);
    ASSERT_EQ(model.aborted, 0);
}

TEST_MAIN("sgp30 hardware model")
    RUN(test_init_sends_iaq_init);
    RUN(test_measure_iaq_default_clean_air);
    RUN(test_measure_iaq_model_values);
    RUN(test_measure_iaq_max_values);
    RUN(test_measure_iaq_increments_counter);
    RUN(test_measure_iaq_sends_correct_cmd);
    RUN(test_get_baseline_default);
    RUN(test_get_baseline_custom);
    RUN(test_set_baseline_round_trip);
    RUN(test_set_baseline_zero);
    RUN(test_set_baseline_max);
    RUN(test_get_serial_default);
    RUN(test_get_serial_custom);
    RUN(test_set_humidity_value);
    RUN(test_set_humidity_disable);
    RUN(test_set_humidity_max);
    RUN(test_model_crc_matches_driver_crc);
    RUN(test_full_workflow);
    RUN(test_iaq_changes_between_reads);
    RUN(test_no_communication_for_the_maximum_duration);
    RUN(test_set_commands_are_busy_too);
    RUN(test_serial_id_needs_0_5_ms);
    RUN(test_data_reads_once);
    RUN(test_bytes_past_the_response_are_ones);
    RUN(test_a_cut_read_ends_the_data);
    RUN(test_a_command_with_no_response_reads_ones);
    RUN(test_data_needs_its_checksum);
    RUN(test_a_short_parameter_is_not_taken);
    RUN(test_driver_never_talks_to_a_busy_sensor);
TEST_MAIN_END()
