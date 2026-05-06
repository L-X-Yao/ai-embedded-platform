/*
 * test_hw_model_sht40.c — tests using hardware model
 *
 * Validates SHT40 driver against a datasheet-accurate model.
 * The model generates CRC-protected responses for each command,
 * enabling round-trip tests: set physical value -> read -> verify.
 */

#include "test_harness.h"
#include "hw_model_sht40.h"
#include "../sht40.h"

static sht40_model_t model;

static const sht40_i2c_t model_i2c = {
    .write    = sht40_model_write,
    .read     = sht40_model_read,
    .delay_ms = sht40_model_delay,
    .ctx      = &model,
};

static sht40_t dev;

static void setup(void)
{
    sht40_model_init(&model);
    sht40_init(&dev, &model_i2c, SHT40_ADDR_A);
    model.write_count = 0;
    model.read_count  = 0;
}

/* --- init --- */

TEST(test_init_reads_serial)
{
    sht40_model_init(&model);
    sht40_model_set_serial(&model, 0xAABBCCDDu);
    sht40_t d;
    sht40_err_t e = sht40_init(&d, &model_i2c, SHT40_ADDR_A);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_EQ(d.serial, 0xAABBCCDDu);
}

TEST(test_init_sends_reset)
{
    sht40_model_init(&model);
    sht40_t d;
    sht40_init(&d, &model_i2c, SHT40_ADDR_A);
    ASSERT_TRUE(model.reset_done);
}

/* --- round-trip: temperature --- */

TEST(test_roundtrip_25c)
{
    setup();
    sht40_model_set_temperature_c(&model, 25.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.1f);
}

TEST(test_roundtrip_0c)
{
    setup();
    sht40_model_set_temperature_c(&model, 0.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.1f);
}

TEST(test_roundtrip_minus40c)
{
    setup();
    sht40_model_set_temperature_c(&model, -40.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.temperature_c, -40.0f, 0.1f);
}

TEST(test_roundtrip_85c)
{
    setup();
    sht40_model_set_temperature_c(&model, 85.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.temperature_c, 85.0f, 0.1f);
}

TEST(test_roundtrip_125c)
{
    setup();
    sht40_model_set_temperature_c(&model, 125.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.temperature_c, 125.0f, 0.1f);
}

/* --- round-trip: humidity --- */

TEST(test_roundtrip_50pct)
{
    setup();
    sht40_model_set_humidity_pct(&model, 50.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.1f);
}

TEST(test_roundtrip_0pct)
{
    setup();
    sht40_model_set_humidity_pct(&model, 0.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.humidity_pct, 0.0f, 0.1f);
}

TEST(test_roundtrip_100pct)
{
    setup();
    sht40_model_set_humidity_pct(&model, 100.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.humidity_pct, 100.0f, 0.1f);
}

/* --- round-trip: combined temperature + humidity --- */

TEST(test_roundtrip_combined)
{
    setup();
    sht40_model_set_temperature_c(&model, 37.5f);
    sht40_model_set_humidity_pct(&model, 65.0f);
    sht40_data_t data;
    sht40_err_t e = sht40_read_high(&dev, &data);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_NEAR(data.temperature_c, 37.5f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 65.0f, 0.1f);
}

/* --- all precision modes produce same result --- */

TEST(test_all_precisions_same_result)
{
    setup();
    sht40_model_set_temperature_c(&model, 22.0f);
    sht40_model_set_humidity_pct(&model, 45.0f);

    sht40_data_t high, med, low;
    ASSERT_EQ(sht40_read_high(&dev, &high), SHT40_OK);
    ASSERT_EQ(sht40_read_medium(&dev, &med), SHT40_OK);
    ASSERT_EQ(sht40_read_low(&dev, &low), SHT40_OK);

    ASSERT_NEAR(high.temperature_c, med.temperature_c, 0.01f);
    ASSERT_NEAR(high.temperature_c, low.temperature_c, 0.01f);
    ASSERT_NEAR(high.humidity_pct, med.humidity_pct, 0.01f);
    ASSERT_NEAR(high.humidity_pct, low.humidity_pct, 0.01f);
}

/* --- serial number round-trip --- */

TEST(test_serial_roundtrip)
{
    setup();
    sht40_model_set_serial(&model, 0xDEADBEEFu);
    uint32_t serial;
    sht40_err_t e = sht40_read_serial(&dev, &serial);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_EQ_HEX(serial, 0xDEADBEEFu);
}

TEST(test_serial_zero)
{
    setup();
    sht40_model_set_serial(&model, 0x00000000u);
    uint32_t serial;
    sht40_err_t e = sht40_read_serial(&dev, &serial);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_EQ(serial, 0x00000000u);
}

TEST(test_serial_max)
{
    setup();
    sht40_model_set_serial(&model, 0xFFFFFFFFu);
    uint32_t serial;
    sht40_err_t e = sht40_read_serial(&dev, &serial);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_EQ_HEX(serial, 0xFFFFFFFFu);
}

/* --- model CRC consistency --- */

TEST(test_model_crc_matches_driver_crc)
{
    /* Verify model CRC and driver CRC produce same results */
    uint8_t data1[] = { 0xBE, 0xEF };
    ASSERT_EQ(sht40_model_crc8(data1, 2), sht40_crc8(data1, 2));

    uint8_t data2[] = { 0x00, 0x00 };
    ASSERT_EQ(sht40_model_crc8(data2, 2), sht40_crc8(data2, 2));

    uint8_t data3[] = { 0xFF, 0xFF };
    ASSERT_EQ(sht40_model_crc8(data3, 2), sht40_crc8(data3, 2));
}

/* --- command tracking --- */

TEST(test_high_sends_fd)
{
    setup();
    sht40_data_t data;
    sht40_read_high(&dev, &data);
    ASSERT_EQ(model.last_cmd, SHT40_CMD_MEASURE_HIGH);
}

TEST(test_medium_sends_f6)
{
    setup();
    sht40_data_t data;
    sht40_read_medium(&dev, &data);
    ASSERT_EQ(model.last_cmd, SHT40_CMD_MEASURE_MEDIUM);
}

TEST(test_low_sends_e0)
{
    setup();
    sht40_data_t data;
    sht40_read_low(&dev, &data);
    ASSERT_EQ(model.last_cmd, SHT40_CMD_MEASURE_LOW);
}

/* --- sequential reads with changing environment --- */

TEST(test_sequential_temp_changes)
{
    setup();
    float temps[] = { -40.0f, -10.0f, 0.0f, 25.0f, 50.0f, 85.0f, 125.0f };
    sht40_data_t data;

    for (int i = 0; i < 7; i++) {
        sht40_model_set_temperature_c(&model, temps[i]);
        sht40_err_t e = sht40_read_high(&dev, &data);
        ASSERT_EQ(e, SHT40_OK);
        ASSERT_NEAR(data.temperature_c, temps[i], 0.1f);
    }
}

TEST(test_sequential_humidity_changes)
{
    setup();
    float hums[] = { 0.0f, 10.0f, 25.0f, 50.0f, 75.0f, 90.0f, 100.0f };
    sht40_data_t data;

    for (int i = 0; i < 7; i++) {
        sht40_model_set_humidity_pct(&model, hums[i]);
        sht40_err_t e = sht40_read_high(&dev, &data);
        ASSERT_EQ(e, SHT40_OK);
        ASSERT_NEAR(data.humidity_pct, hums[i], 0.1f);
    }
}

/* --- soft reset via model --- */

TEST(test_soft_reset_via_model)
{
    setup();
    model.reset_done = false;
    sht40_err_t e = sht40_soft_reset(&dev);
    ASSERT_EQ(e, SHT40_OK);
    ASSERT_TRUE(model.reset_done);
    ASSERT_EQ(model.last_cmd, SHT40_CMD_SOFT_RESET);
}

TEST_MAIN("sht40 hardware model")
    RUN(test_init_reads_serial);
    RUN(test_init_sends_reset);
    RUN(test_roundtrip_25c);
    RUN(test_roundtrip_0c);
    RUN(test_roundtrip_minus40c);
    RUN(test_roundtrip_85c);
    RUN(test_roundtrip_125c);
    RUN(test_roundtrip_50pct);
    RUN(test_roundtrip_0pct);
    RUN(test_roundtrip_100pct);
    RUN(test_roundtrip_combined);
    RUN(test_all_precisions_same_result);
    RUN(test_serial_roundtrip);
    RUN(test_serial_zero);
    RUN(test_serial_max);
    RUN(test_model_crc_matches_driver_crc);
    RUN(test_high_sends_fd);
    RUN(test_medium_sends_f6);
    RUN(test_low_sends_e0);
    RUN(test_sequential_temp_changes);
    RUN(test_sequential_humidity_changes);
    RUN(test_soft_reset_via_model);
TEST_MAIN_END()
