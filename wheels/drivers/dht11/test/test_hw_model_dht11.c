// SPDX-License-Identifier: MIT
/*
 * test_hw_model_dht11.c -- tests using the DHT11 bus timing model
 *
 * The driver runs against a sensor whose pin level comes from the manual's
 * timings, so a wrong start pulse or sample point fails here. The mock in
 * test_dht11.c replays levels in the driver's own read order and cannot show it.
 */

#include "test_harness.h"
#include "hw_model_dht11.h"
#include "../dht11.h"

static dht11_model_t model;

static const dht11_gpio_t model_gpio = {
    .set_output = dht11_model_set_output,
    .set_input  = dht11_model_set_input,
    .write_pin  = dht11_model_write_pin,
    .read_pin   = dht11_model_read_pin,
    .delay_us   = dht11_model_delay_us,
    .ctx        = &model,
};

static dht11_t dev;

static void setup(void)
{
    dht11_model_init(&model);
    (void)dht11_init(&dev, &model_gpio);
}

/* A start signal of the given length, then the bus released, then the wait
 * of the model, so that the frame has just begun. */
static void send_start(uint32_t low_us)
{
    dht11_model_set_output(&model);
    dht11_model_write_pin(&model, false);
    dht11_model_delay_us(&model, low_us);
    dht11_model_set_input(&model);
    dht11_model_delay_us(&model, model.response_delay_us);
}

/* --- what the model itself must do --- */

/* p.6: the bus idles high through the pull-up. */
TEST(test_model_line_idles_high)
{
    setup();
    ASSERT_TRUE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1000);
    ASSERT_TRUE(dht11_model_read_pin(&model));
}

/* p.6: "low retention time can't be less than 18ms", and the figure: ">18ms". */
TEST(test_model_needs_a_start_longer_than_18ms)
{
    setup();
    send_start(18000);
    ASSERT_EQ(model.frame_count, 0);
    ASSERT_TRUE(dht11_model_read_pin(&model));

    send_start(18001);
    ASSERT_EQ(model.frame_count, 1);
    ASSERT_FALSE(dht11_model_read_pin(&model));
}

/* p.6 and p.7: "the output low as 80 microseconds response signal, followed by
 * the output of 80 micro-notify". Then each bit, here 1 then 0, from the first
 * byte 1000 0000. */
TEST(test_model_frame_timing)
{
    setup();
    dht11_model_set_measured(&model, 0x80, 0, 0, 0);
    send_start(20000);

    ASSERT_FALSE(dht11_model_read_pin(&model));     /* response low */
    dht11_model_delay_us(&model, 79);
    ASSERT_FALSE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1);
    ASSERT_TRUE(dht11_model_read_pin(&model));      /* response high */
    dht11_model_delay_us(&model, 79);
    ASSERT_TRUE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1);
    ASSERT_FALSE(dht11_model_read_pin(&model));     /* bit 1: 50 us low */
    dht11_model_delay_us(&model, 49);
    ASSERT_FALSE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1);
    ASSERT_TRUE(dht11_model_read_pin(&model));      /* bit 1: 70 us high */
    dht11_model_delay_us(&model, 69);
    ASSERT_TRUE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1);
    ASSERT_FALSE(dht11_model_read_pin(&model));     /* bit 0: 50 us low */
    dht11_model_delay_us(&model, 50);
    ASSERT_TRUE(dht11_model_read_pin(&model));      /* bit 0: 28 us high */
    dht11_model_delay_us(&model, 27);
    ASSERT_TRUE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1);
    ASSERT_FALSE(dht11_model_read_pin(&model));
}

/* p.7: "the continued output low 50 microseconds after the entry into the
 * state, due to the pull-up resistor attendant goes high." */
TEST(test_model_frame_ends_with_50us_low)
{
    setup();
    dht11_model_set_measured(&model, 0x00, 0x00, 0x00, 0x00);
    send_start(20000);

    /* response, then 40 bits of 0 at 50 + 28 us */
    dht11_model_delay_us(&model, 160u + 40u * 78u);
    ASSERT_FALSE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 49);
    ASSERT_FALSE(dht11_model_read_pin(&model));
    dht11_model_delay_us(&model, 1);
    ASSERT_TRUE(dht11_model_read_pin(&model));
    ASSERT_FALSE(model.frame_active);
}

/* --- the driver against the model --- */

/* p.5 Example One: 0011 0101, 0000 0000, 0001 1000, 0000 0000, parity
 * 0100 1101: "Humidity: 0011 0101=35H=53%RH", "Temperature:0001 1000=18H=24" */
TEST(test_read_manual_example_one)
{
    setup();
    dht11_model_set_measured(&model, 0x35, 0x00, 0x18, 0x00);
    ASSERT_EQ_HEX(model.recorded[4], 0x4D);

    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 53);
    ASSERT_EQ(data.humidity_dec, 0);
    ASSERT_EQ(data.temperature_int, 24);
    ASSERT_EQ(data.temperature_dec, 0);
}

/* p.5 Example Two: the same data with parity 0100 1001. "The received data is
 * not correct, give up, again receiving data." */
TEST(test_read_manual_example_two_is_refused)
{
    setup();
    dht11_model_set_measured(&model, 0x35, 0x00, 0x18, 0x00);
    model.recorded[4] = 0x49;

    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_ERR_CHECKSUM);
}

/* Every value of one byte, so that each bit is read as both 0 and 1, and the
 * byte order shows: only the temperature byte changes. */
TEST(test_read_every_bit_at_both_values)
{
    static const uint32_t zero_high_us[] = { 26u, 28u };

    for (unsigned t = 0; t < 2u; t++) {
        for (unsigned v = 0; v < 256u; v += 17u) {
            setup();
            model.zero_high_us = zero_high_us[t];
            dht11_model_set_measured(&model, 0x5A, 0xA5, (uint8_t)v, 0x0F);

            dht11_data_t data;
            ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
            ASSERT_EQ_HEX(data.humidity_int, 0x5A);
            ASSERT_EQ_HEX(data.humidity_dec, 0xA5);
            ASSERT_EQ_HEX(data.temperature_int, v);
            ASSERT_EQ_HEX(data.temperature_dec, 0x0F);
        }
    }
}

/* p.5: parity is "the result of the end of eight", so the sum may wrap. */
TEST(test_read_with_a_parity_that_wraps)
{
    setup();
    dht11_model_set_measured(&model, 0xFF, 0xFF, 0xFF, 0xFF);
    ASSERT_EQ_HEX(model.recorded[4], 0xFC);

    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ_HEX(data.humidity_int, 0xFF);
}

/* p.6: "The host reads temperature and humidity data from DHT11 always previous
 * measurements", and "please read twice in a row is the second time in real
 * time temperature and humidity values." */
TEST(test_read_gives_the_previous_measurement)
{
    setup();
    dht11_model_set_measured(&model, 40, 0, 20, 0);
    dht11_model_set_environment(&model, 60, 0, 30, 0);

    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 40);

    /* the frame's end signal has to go before the sensor measures */
    dht11_model_delay_us(&model, 1000);
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 60);
    ASSERT_EQ(data.temperature_int, 30);
}

/* p.6: the start is answered only if it is long enough. */
TEST(test_read_start_signal_is_long_enough)
{
    setup();
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(model.frame_count, 1);
}

/* p.6: "then the microprocessor I / O is set to enter the state, due to the
 * pull-up resistor, the microprocessor I / O that the data lines DHT11 also
 * will go high". The wait before the answer has no number in the manual, so the
 * host must be off the bus at once, however soon the sensor answers. */
TEST(test_read_never_drives_against_the_sensor)
{
    static const uint32_t response_delay_us[] = { 1u, 10u, 20u, 40u };

    for (unsigned i = 0; i < 4u; i++) {
        setup();
        model.response_delay_us = response_delay_us[i];
        dht11_model_set_measured(&model, 0x35, 0x00, 0x18, 0x00);

        dht11_data_t data;
        ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
        ASSERT_EQ(data.humidity_int, 53);
        ASSERT_FALSE(model.contention);
    }
}

/* With no sensor the pull-up holds the line high and the first wait ends. */
TEST(test_read_with_no_sensor_times_out)
{
    setup();
    model.present = false;
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_ERR_TIMEOUT);
}

/* p.6: "power after DHT11 1S to wait to cross the unstable state during this
 * period can't send any commands". */
TEST(test_read_in_the_first_second_gets_no_answer)
{
    setup();
    dht11_model_set_environment(&model, 53, 0, 24, 0);
    dht11_model_power_up(&model);

    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_ERR_TIMEOUT);
    ASSERT_EQ(model.frame_count, 0);

    dht11_model_delay_us(&model, DHT11_MODEL_POWER_UP_US);
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 53);
}

TEST_MAIN("dht11 hardware model")
    RUN(test_model_line_idles_high);
    RUN(test_model_needs_a_start_longer_than_18ms);
    RUN(test_model_frame_timing);
    RUN(test_model_frame_ends_with_50us_low);
    RUN(test_read_manual_example_one);
    RUN(test_read_manual_example_two_is_refused);
    RUN(test_read_every_bit_at_both_values);
    RUN(test_read_with_a_parity_that_wraps);
    RUN(test_read_gives_the_previous_measurement);
    RUN(test_read_start_signal_is_long_enough);
    RUN(test_read_never_drives_against_the_sensor);
    RUN(test_read_with_no_sensor_times_out);
    RUN(test_read_in_the_first_second_gets_no_answer);
TEST_MAIN_END()
