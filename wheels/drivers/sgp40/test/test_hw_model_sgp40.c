// SPDX-License-Identifier: MIT
/*
 * test_hw_model_sgp40.c -- tests using the SGP40 hardware model
 *
 * Drives the driver against the command protocol of the Sensirion SGP40
 * datasheet v1.2: raw I2C commands, a NACK while a command runs, and CRC
 * bytes on every data word.
 */

#include "test_harness.h"
#include "hw_model_sgp40.h"
#include "../sgp40.h"

static sgp40_model_t model;

static const sgp40_i2c_t model_i2c = {
    .write    = sgp40_model_write,
    .read     = sgp40_model_read,
    .delay_ms = sgp40_model_delay,
    .ctx      = &model,
};

static void setup(void)
{
    sgp40_model_init(&model);
}

static bool send(const uint8_t *cmd, uint16_t len)
{
    return model_i2c.write(&model, SGP40_DEV_ADDR, cmd, len);
}

static const uint8_t self_test_cmd[2] = { 0x28, 0x0E };
/* Table 9 on p13, with the default words that disable compensation */
static const uint8_t measure_cmd[8] = { 0x26, 0x0F, 0x80, 0x00, 0xA2,
                                        0x66, 0x66, 0x93 };

/* --- self-test --- */

/* Table 13 on p14: "0xD4 0xXX: all tests passed successfully (ignore 0xXX
 * byte)" and "0x4B 0xXX: one or more tests have failed" */
TEST(test_init_reads_the_first_byte_only)
{
    static const uint8_t any[] = { 0x00, 0x5A, 0xD4, 0x4B, 0xFF };
    for (unsigned i = 0; i < sizeof(any); i++) {
        sgp40_t dev;

        setup();
        model.self_test_low = any[i];
        ASSERT_EQ(sgp40_init(&dev, &model_i2c), SGP40_OK);
        ASSERT_EQ(model.self_test_count, 1);

        setup();
        model.self_test_low = any[i];
        model.self_test_pass = false;
        ASSERT_EQ(sgp40_init(&dev, &model_i2c), SGP40_ERR_SELFTEST);
    }
}

/* --- timing --- */

/* "After 320 ms, the master can read a fixed data pattern" (p10), and "When
 * the measurement is in progress, no communication with the sensor is
 * possible and the sensor aborts the communication with a NACK" (p11) */
TEST(test_self_test_result_comes_after_320ms)
{
    uint8_t buf[3];
    setup();
    ASSERT_TRUE(send(self_test_cmd, 2));
    ASSERT_FALSE(model_i2c.read(&model, SGP40_DEV_ADDR, buf, 3));
    sgp40_model_delay(&model, 319);
    ASSERT_FALSE(model_i2c.read(&model, SGP40_DEV_ADDR, buf, 3));
    sgp40_model_delay(&model, 1);
    ASSERT_TRUE(model_i2c.read(&model, SGP40_DEV_ADDR, buf, 3));
    ASSERT_EQ_HEX(buf[0], 0xD4);
}

/* Table 8 on p12: 30 ms at most */
TEST(test_measure_result_comes_after_30ms)
{
    uint8_t buf[3];
    setup();
    ASSERT_TRUE(send(measure_cmd, 8));
    sgp40_model_delay(&model, 29);
    ASSERT_FALSE(model_i2c.read(&model, SGP40_DEV_ADDR, buf, 3));
    sgp40_model_delay(&model, 1);
    ASSERT_TRUE(model_i2c.read(&model, SGP40_DEV_ADDR, buf, 3));
    ASSERT_EQ(((uint16_t)buf[0] << 8) | buf[1], model.sraw_value);
    ASSERT_EQ(buf[2], sgp40_crc8(buf, 2));
}

TEST(test_busy_sensor_refuses_commands)
{
    setup();
    ASSERT_TRUE(send(measure_cmd, 8));
    ASSERT_FALSE(send(self_test_cmd, 2));
    sgp40_model_delay(&model, 30);
    ASSERT_TRUE(send(self_test_cmd, 2));
}

TEST(test_nothing_to_read_without_a_command)
{
    uint8_t buf[3];
    setup();
    ASSERT_FALSE(model_i2c.read(&model, SGP40_DEV_ADDR, buf, 3));
}

/* "The sensor's I2C address is 89 (decimal; hex.: 0x59)", p11 */
TEST(test_only_address_0x59_answers)
{
    uint8_t buf[3];
    setup();
    ASSERT_FALSE(model_i2c.write(&model, 0x58, self_test_cmd, 2));
    ASSERT_TRUE(send(self_test_cmd, 2));
    sgp40_model_delay(&model, 320);
    ASSERT_FALSE(model_i2c.read(&model, 0x58, buf, 3));
}

/* --- the heater --- */

/* "If this command is called when the sensor is in idle mode, the sensor
 * returns to idle mode after the test", and from measurement mode "the
 * hotplate remains switched on thereafter" (p10); 0x3615 turns it off (p12) */
TEST(test_heater_follows_the_commands)
{
    static const uint8_t heater_off[2] = { 0x36, 0x15 };
    setup();
    ASSERT_TRUE(send(self_test_cmd, 2));
    sgp40_model_delay(&model, 320);
    ASSERT_FALSE(model.heater_on);

    ASSERT_TRUE(send(measure_cmd, 8));
    ASSERT_TRUE(model.heater_on);
    sgp40_model_delay(&model, 30);

    ASSERT_TRUE(send(self_test_cmd, 2));
    sgp40_model_delay(&model, 320);
    ASSERT_TRUE(model.heater_on);

    ASSERT_TRUE(send(heater_off, 2));
    ASSERT_FALSE(model.heater_on);
}

/* --- the driver on the wire --- */

/* Table 9 on p13: default values, and Table 11 for the answer */
TEST(test_measure_raw_sends_the_default_words)
{
    static const uint8_t params[6] = { 0x80, 0x00, 0xA2, 0x66, 0x66, 0x93 };
    setup();
    sgp40_t dev;
    ASSERT_EQ(sgp40_init(&dev, &model_i2c), SGP40_OK);

    model.sraw_value = 0x1234;
    sgp40_data_t data;
    ASSERT_EQ(sgp40_measure_raw(&dev, &data), SGP40_OK);
    ASSERT_EQ_HEX(data.sraw_voc, 0x1234);
    ASSERT_EQ(model.measure_count, 1);
    ASSERT_MEMEQ_N(model.last_params, params, 6);
    ASSERT_EQ(model.bad_crc_count, 0);
}

/* Table 10 on p13: the limits and their CRC bytes */
TEST(test_measure_comp_sends_crc_bytes_of_the_manual)
{
    static const uint8_t low[6]  = { 0x00, 0x00, 0x81, 0x00, 0x00, 0x81 };
    static const uint8_t high[6] = { 0xFF, 0xFF, 0xAC, 0xFF, 0xFF, 0xAC };
    setup();
    sgp40_t dev;
    ASSERT_EQ(sgp40_init(&dev, &model_i2c), SGP40_OK);

    sgp40_data_t data;
    ASSERT_EQ(sgp40_measure_raw_comp(&dev, &data, 0x0000, 0x0000), SGP40_OK);
    ASSERT_MEMEQ_N(model.last_params, low, 6);
    ASSERT_EQ(sgp40_measure_raw_comp(&dev, &data, 0xFFFF, 0xFFFF), SGP40_OK);
    ASSERT_MEMEQ_N(model.last_params, high, 6);
    ASSERT_EQ(model.bad_crc_count, 0);
}

/* A word is "followed by" its CRC (p11): the model has to notice a bad one */
TEST(test_model_notices_a_bad_parameter_crc)
{
    uint8_t cmd[8];
    memcpy(cmd, measure_cmd, sizeof(cmd));
    cmd[4] = 0x00;
    setup();
    send(cmd, 8);
    ASSERT_EQ(model.bad_crc_count, 1);
}

TEST(test_model_refuses_unknown_commands)
{
    static const uint8_t cmd[2] = { 0x12, 0x34 };
    setup();
    ASSERT_FALSE(send(cmd, 2));
    ASSERT_EQ(model.bad_cmd_count, 1);
}

TEST_MAIN("sgp40 hardware model")
    RUN(test_init_reads_the_first_byte_only);
    RUN(test_self_test_result_comes_after_320ms);
    RUN(test_measure_result_comes_after_30ms);
    RUN(test_busy_sensor_refuses_commands);
    RUN(test_nothing_to_read_without_a_command);
    RUN(test_only_address_0x59_answers);
    RUN(test_heater_follows_the_commands);
    RUN(test_measure_raw_sends_the_default_words);
    RUN(test_measure_comp_sends_crc_bytes_of_the_manual);
    RUN(test_model_notices_a_bad_parameter_crc);
    RUN(test_model_refuses_unknown_commands);
TEST_MAIN_END()
