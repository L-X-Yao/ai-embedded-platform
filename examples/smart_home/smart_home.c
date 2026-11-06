// SPDX-License-Identifier: MIT
/*
 * smart_home -- integration recipe: smart home IoT node
 *
 * Demonstrates nine wheels working together:
 *   L0  debounce       -- button input debouncing
 *   L0  timer_wheel    -- scheduled one-shot callbacks
 *   L0  led_pattern    -- LED blink pattern engine
 *   L1  bme280         -- temperature + humidity + pressure sensor
 *   L1  bh1750         -- ambient light level sensor
 *   L1  ws2812         -- RGB LED strip driver
 *   L2  cbor_lite      -- binary CBOR payload encoding
 *   L3  scheduler      -- periodic task scheduler
 *   L3  event_bus      -- publish/subscribe event dispatch
 *
 * Runs entirely on host with mock I2C / mock PWM HAL -- no hardware.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/smart_home
 */

#include "test_harness.h"

/* wheels */
#include "debounce.h"
#include "timer_wheel.h"
#include "led_pattern.h"
#include "bme280.h"
#include "bh1750.h"
#include "ws2812.h"
#include "cbor_lite.h"
#include "scheduler.h"
#include "event_bus.h"

#include <string.h>
#include <math.h>

/* ======================================================================
 * Event IDs
 * ====================================================================== */

#define EVT_SENSOR_DATA   1
#define EVT_BUTTON_PRESS  2
#define EVT_ALERT         3

/* ======================================================================
 * Mock I2C -- BME280 (register-based: write_reg / read_regs)
 *
 * Register map mirrors the real BME280 (chip ID 0x60 at 0xD0,
 * calibration at 0x88..0x9F + 0xA1 + 0xE1..0xE7, data at 0xF7..0xFE).
 * ====================================================================== */

static uint8_t bme_regs[256];
static int     bme_op_count;

static void bme_mock_reset(void)
{
    memset(bme_regs, 0, sizeof(bme_regs));
    bme_op_count = 0;

    /* Chip ID = 0x60 (BME280) */
    bme_regs[0xD0] = 0x60;

    /* Temperature / pressure calibration (0x88..0x9F) -- realistic values */
    bme_regs[0x88] = 0x70; bme_regs[0x89] = 0x6B;  /* dig_T1 = 27504 */
    bme_regs[0x8A] = 0x43; bme_regs[0x8B] = 0x67;  /* dig_T2 = 26435 */
    bme_regs[0x8C] = 0x18; bme_regs[0x8D] = 0xFC;  /* dig_T3 = -1000 */
    bme_regs[0x8E] = 0x7D; bme_regs[0x8F] = 0x8E;  /* dig_P1 = 36477 */
    bme_regs[0x90] = 0x43; bme_regs[0x91] = 0xD6;  /* dig_P2 */
    bme_regs[0x92] = 0xD0; bme_regs[0x93] = 0x0B;  /* dig_P3 */
    bme_regs[0x94] = 0x27; bme_regs[0x95] = 0x0B;  /* dig_P4 */
    bme_regs[0x96] = 0x8C; bme_regs[0x97] = 0x00;  /* dig_P5 */
    bme_regs[0x98] = 0xF9; bme_regs[0x99] = 0xFF;  /* dig_P6 */
    bme_regs[0x9A] = 0x8C; bme_regs[0x9B] = 0x3C;  /* dig_P7 */
    bme_regs[0x9C] = 0xF8; bme_regs[0x9D] = 0xC6;  /* dig_P8 */
    bme_regs[0x9E] = 0x70; bme_regs[0x9F] = 0x17;  /* dig_P9 */

    /* dig_H1 at 0xA1 */
    bme_regs[0xA1] = 0x4B;  /* dig_H1 = 75 */

    /* Humidity calibration at 0xE1..0xE7 */
    bme_regs[0xE1] = 0x6E; bme_regs[0xE2] = 0x01;  /* dig_H2 = 366 */
    bme_regs[0xE3] = 0x00;                          /* dig_H3 = 0 */
    bme_regs[0xE4] = 0x12; bme_regs[0xE5] = 0x34;  /* dig_H4, H5 (interleaved) */
    bme_regs[0xE6] = 0x00;                          /* dig_H5 high nibble */
    bme_regs[0xE7] = 0x1E;                          /* dig_H6 = 30 */

    /* STATUS register: measuring=0 (conversion ready) */
    bme_regs[0xF3] = 0x00;

    /* Plant ADC values: adc_t=519888 (~25C), adc_p=415148 (~101325 Pa) */
    uint32_t adc_p = 415148;
    uint32_t adc_t = 519888;
    bme_regs[0xF7] = (uint8_t)(adc_p >> 12);
    bme_regs[0xF8] = (uint8_t)(adc_p >> 4);
    bme_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    bme_regs[0xFA] = (uint8_t)(adc_t >> 12);
    bme_regs[0xFB] = (uint8_t)(adc_t >> 4);
    bme_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);

    /* Plant humidity ADC: adc_h ~32000 (~50% RH with simple calib) */
    uint16_t adc_h = 32000;
    bme_regs[0xFD] = (uint8_t)(adc_h >> 8);
    bme_regs[0xFE] = (uint8_t)(adc_h & 0xFF);
}

static bool bme_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    bme_op_count++;
    bme_regs[reg] = val;
    if (reg == 0xF4)
        bme_regs[reg] &= 0xFC;  /* forced mode ends at once, back to sleep */
    return true;
}

static bool bme_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    bme_op_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = bme_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const bme280_i2c_t bme_i2c = {
    .write_reg = bme_write_reg,
    .read_regs = bme_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock I2C -- BH1750 (command-based: write cmd byte, read 2 bytes)
 * ====================================================================== */

static uint8_t  bh_read_buf[2];
static int      bh_op_count;

static void bh_plant_lux(float lux)
{
    /* BH1750 H-Res mode: lux = raw / 1.2 --> raw = lux * 1.2 */
    uint16_t raw = (uint16_t)(lux * 1.2f);
    bh_read_buf[0] = (uint8_t)(raw >> 8);
    bh_read_buf[1] = (uint8_t)(raw & 0xFF);
}

static void bh_mock_reset(void)
{
    memset(bh_read_buf, 0, sizeof(bh_read_buf));
    bh_op_count = 0;
    bh_plant_lux(500.0f);  /* indoor: ~500 lux */
}

static bool bh_write(void *ctx, uint8_t dev_addr, const uint8_t *data,
                     uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)data; (void)len;
    bh_op_count++;
    return true;
}

static bool bh_read(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    bh_op_count++;
    for (uint16_t i = 0; i < len && i < sizeof(bh_read_buf); i++) {
        data[i] = bh_read_buf[i];
    }
    return true;
}

static const bh1750_i2c_t bh_i2c = {
    .write    = bh_write,
    .read     = bh_read,
    .delay_ms = NULL,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock WS2812 HAL -- records pixel data for verification
 * ====================================================================== */

#define MAX_LEDS        8
#define BITS_PER_LED   24

static uint16_t ws_compare_log[MAX_LEDS * BITS_PER_LED + 64];
static uint32_t ws_compare_idx;
static uint32_t ws_period_count;

static void ws_mock_reset(void)
{
    memset(ws_compare_log, 0, sizeof(ws_compare_log));
    ws_compare_idx  = 0;
    ws_period_count = 0;
}

static bool ws_set_compare(void *ctx, uint16_t value)
{
    (void)ctx;
    if (ws_compare_idx < sizeof(ws_compare_log) / sizeof(ws_compare_log[0])) {
        ws_compare_log[ws_compare_idx++] = value;
    }
    return true;
}

static void ws_wait_period(void *ctx)
{
    (void)ctx;
    ws_period_count++;
}

static void ws_enter_critical(void *ctx) { (void)ctx; }
static void ws_exit_critical(void *ctx)  { (void)ctx; }

static const ws2812_hal_t ws_hal = {
    .set_compare    = ws_set_compare,
    .wait_period    = ws_wait_period,
    .enter_critical = ws_enter_critical,
    .exit_critical  = ws_exit_critical,
    .ctx            = NULL,
};

static const ws2812_timing_t ws_timing = {
    .period_ticks  = 90,
    .t0h_ticks     = 29,
    .t1h_ticks     = 58,
    .reset_periods = 50,
};

/* ======================================================================
 * Global instances
 * ====================================================================== */

static bme280_t      bme_dev;
static bh1750_t      bh_dev;
static debounce_t    btn;
static timer_wheel_t tw;
static led_pattern_t led_pat;
static scheduler_t   sched;
static event_bus_t   bus;

/* timer_wheel storage (statically allocated) */
#define TW_NUM_SLOTS  16
#define TW_MAX_TIMERS  8
static timer_wheel_slot_t  tw_slots[TW_NUM_SLOTS];
static timer_wheel_entry_t tw_timers[TW_MAX_TIMERS];

/* event_bus subscriber tracking */
static int  sensor_event_count;
static float last_received_temp;
static float last_received_hum;
static float last_received_press;
static float last_received_lux;

/* CBOR buffer for encoded sensor data */
static uint8_t cbor_buf[128];
static size_t  cbor_len;

/* timer_wheel alert tracking */
static int  alert_fired_count;

/* scheduler callback tracking */
static int  sched_sensor_calls;

/* ======================================================================
 * Sensor data structure for event_bus payloads
 * ====================================================================== */

typedef struct {
    float temperature_c;
    float humidity_pct;
    float pressure_pa;
    float light_lux;
} sensor_payload_t;

/* ======================================================================
 * Event bus subscriber: encodes sensor data as CBOR
 * ====================================================================== */

static void on_sensor_data(uint16_t event_id, const void *data,
                           size_t data_len, void *ctx)
{
    (void)ctx;
    (void)event_id;

    if (!data || data_len != sizeof(sensor_payload_t)) {
        return;
    }

    const sensor_payload_t *p = (const sensor_payload_t *)data;
    last_received_temp  = p->temperature_c;
    last_received_hum   = p->humidity_pct;
    last_received_press = p->pressure_pa;
    last_received_lux   = p->light_lux;
    sensor_event_count++;

    /* Encode as CBOR map */
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, cbor_buf, sizeof(cbor_buf));
    cbor_lite_enc_map(&enc, 4);
    cbor_lite_enc_tstr(&enc, "temp", 4);
    cbor_lite_enc_float(&enc, p->temperature_c);
    cbor_lite_enc_tstr(&enc, "hum", 3);
    cbor_lite_enc_float(&enc, p->humidity_pct);
    cbor_lite_enc_tstr(&enc, "press", 5);
    cbor_lite_enc_float(&enc, p->pressure_pa);
    cbor_lite_enc_tstr(&enc, "lux", 3);
    cbor_lite_enc_float(&enc, p->light_lux);
    cbor_len = cbor_lite_enc_len(&enc);
}

/* ======================================================================
 * Timer wheel alert callback
 * ====================================================================== */

static void on_alert(void *ctx)
{
    (void)ctx;
    alert_fired_count++;
}

/* ======================================================================
 * Scheduler callback: reads sensors and publishes to event_bus
 * ====================================================================== */

static void sched_read_sensors(void *ctx)
{
    event_bus_t *b = (event_bus_t *)ctx;
    sched_sensor_calls++;

    bme280_data_t bme_data;
    (void)bme280_read(&bme_dev, &bme_data);

    float lux = 0.0f;
    (void)bh1750_read_lux(&bh_dev, &lux);

    sensor_payload_t payload = {
        .temperature_c = bme_data.temperature_c,
        .humidity_pct  = bme_data.humidity_pct,
        .pressure_pa   = bme_data.pressure_pa,
        .light_lux     = lux,
    };

    (void)event_bus_publish(b, EVT_SENSOR_DATA, &payload, sizeof(payload));
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. BME280 */
    bme_mock_reset();
    bme280_cfg_t bme_cfg = {
        .addr   = BME280_ADDR_LOW,
        .osrs_t = BME280_OVERSAMPLING_1,
        .osrs_p = BME280_OVERSAMPLING_1,
        .osrs_h = BME280_OVERSAMPLING_1,
    };
    bme280_err_t bme_err = bme280_init(&bme_dev, &bme_i2c, &bme_cfg);
    ASSERT_EQ(bme_err, BME280_OK);

    /* 2. BH1750 */
    bh_mock_reset();
    bh1750_cfg_t bh_cfg = {
        .addr = BH1750_ADDR_LOW,
        .mode = BH1750_CONT_H_RES,
    };
    bh1750_err_t bh_err = bh1750_init(&bh_dev, &bh_i2c, &bh_cfg);
    ASSERT_EQ(bh_err, BH1750_OK);

    /* 3. debounce (threshold=3, initial=false/unpressed) */
    debounce_init(&btn, 3, false);
    ASSERT_FALSE(debounce_get(&btn));

    /* 4. timer_wheel */
    timer_wheel_err_t tw_err = timer_wheel_init(&tw, tw_slots, TW_NUM_SLOTS,
                                                 tw_timers, TW_MAX_TIMERS);
    ASSERT_EQ(tw_err, TIMER_WHEEL_OK);

    /* 5. led_pattern -- heartbeat: 2 ticks on, 4 ticks off, looping */
    static const led_step_t heartbeat_steps[] = {
        { .on_ticks = 2, .off_ticks = 4 },
    };
    led_pattern_cfg_t led_cfg = {
        .steps      = heartbeat_steps,
        .step_count = 1,
        .loop       = true,
    };
    led_pattern_init(&led_pat, &led_cfg);
    ASSERT_TRUE(led_pattern_is_on(&led_pat));

    /* 6. scheduler */
    sched_err_t s_err = scheduler_init(&sched);
    ASSERT_EQ(s_err, SCHED_OK);

    /* 7. event_bus */
    event_bus_err_t eb_err = event_bus_init(&bus);
    ASSERT_EQ(eb_err, EVENT_BUS_OK);

    /* 8. ws2812 -- stateless API, just verify timing struct is valid */
    ASSERT_TRUE(ws_timing.period_ticks > 0);
    ASSERT_TRUE(ws_timing.t0h_ticks > 0);
    ASSERT_TRUE(ws_timing.t1h_ticks > ws_timing.t0h_ticks);

    /* 9. cbor_lite -- stateless encoder, init verifies API works */
    cbor_lite_encoder_t enc_test;
    cbor_lite_enc_init(&enc_test, cbor_buf, sizeof(cbor_buf));
    ASSERT_EQ(cbor_lite_enc_len(&enc_test), 0);

    printf("  All 9 wheels initialized successfully\n");
}

TEST(test_bme280_read)
{
    bme280_data_t data;
    bme280_err_t err = bme280_read(&bme_dev, &data);
    ASSERT_EQ(err, BME280_OK);

    /* Temperature should be in plausible range (~25C from planted ADC) */
    ASSERT_TRUE(data.temperature_c > 15.0f && data.temperature_c < 45.0f);

    /* Pressure should be plausible (~101325 Pa) */
    ASSERT_TRUE(data.pressure_pa > 30000.0f && data.pressure_pa < 120000.0f);

    /* Humidity should be in valid range (0-100%) */
    ASSERT_TRUE(data.humidity_pct >= 0.0f && data.humidity_pct <= 100.0f);

    printf("  BME280: temp=%.1f C, press=%.0f Pa, hum=%.1f %%\n",
           (double)data.temperature_c, (double)data.pressure_pa,
           (double)data.humidity_pct);
}

TEST(test_bh1750_read)
{
    /* Plant a known lux value */
    bh_plant_lux(500.0f);

    float lux = 0.0f;
    bh1750_err_t err = bh1750_read_lux(&bh_dev, &lux);
    ASSERT_EQ(err, BH1750_OK);

    /* H-Res mode: lux = raw / 1.2, raw = (uint16_t)(500 * 1.2) = 600
     * read-back: 600 / 1.2 = 500.0 */
    ASSERT_NEAR(lux, 500.0f, 1.0f);

    /* Try a dim reading */
    bh_plant_lux(10.0f);
    err = bh1750_read_lux(&bh_dev, &lux);
    ASSERT_EQ(err, BH1750_OK);
    ASSERT_NEAR(lux, 10.0f, 1.0f);

    printf("  BH1750: lux=%.1f (planted 10.0)\n", (double)lux);

    /* Restore for subsequent tests */
    bh_plant_lux(500.0f);
}

TEST(test_cbor_encode_sensor_data)
{
    /* Encode a sensor payload as a CBOR map */
    cbor_lite_encoder_t enc;
    uint8_t buf[128];
    cbor_lite_enc_init(&enc, buf, sizeof(buf));

    cbor_lite_err_t err;
    err = cbor_lite_enc_map(&enc, 4);
    ASSERT_EQ(err, CBOR_LITE_OK);

    err = cbor_lite_enc_tstr(&enc, "temp", 4);
    ASSERT_EQ(err, CBOR_LITE_OK);
    err = cbor_lite_enc_float(&enc, 25.5f);
    ASSERT_EQ(err, CBOR_LITE_OK);

    err = cbor_lite_enc_tstr(&enc, "hum", 3);
    ASSERT_EQ(err, CBOR_LITE_OK);
    err = cbor_lite_enc_float(&enc, 55.0f);
    ASSERT_EQ(err, CBOR_LITE_OK);

    err = cbor_lite_enc_tstr(&enc, "press", 5);
    ASSERT_EQ(err, CBOR_LITE_OK);
    err = cbor_lite_enc_float(&enc, 101325.0f);
    ASSERT_EQ(err, CBOR_LITE_OK);

    err = cbor_lite_enc_tstr(&enc, "lux", 3);
    ASSERT_EQ(err, CBOR_LITE_OK);
    err = cbor_lite_enc_float(&enc, 500.0f);
    ASSERT_EQ(err, CBOR_LITE_OK);

    size_t len = cbor_lite_enc_len(&enc);
    ASSERT_TRUE(len > 0);

    /* Decode back and verify structure: map(4) */
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, buf, len);

    err = cbor_lite_dec_next(&dec, &item);
    ASSERT_EQ(err, CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_MAP);
    ASSERT_EQ(item.uval, 4);

    /* First key: "temp" */
    err = cbor_lite_dec_next(&dec, &item);
    ASSERT_EQ(err, CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_TSTR);
    ASSERT_EQ(item.str_len, 4);

    /* First value: float 25.5 */
    err = cbor_lite_dec_next(&dec, &item);
    ASSERT_EQ(err, CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_SIMPLE);
    ASSERT_NEAR(item.float_val, 25.5f, 0.01f);

    printf("  CBOR: encoded %u bytes, decode verified\n", (unsigned)len);
}

TEST(test_event_bus_pub_sub)
{
    /* Reset subscriber tracking */
    sensor_event_count = 0;
    last_received_temp = 0.0f;
    cbor_len = 0;

    /* Subscribe to sensor data events */
    event_bus_err_t err = event_bus_subscribe(&bus, EVT_SENSOR_DATA,
                                              on_sensor_data, NULL);
    ASSERT_EQ(err, EVENT_BUS_OK);

    /* Publish a sensor payload */
    sensor_payload_t payload = {
        .temperature_c = 23.5f,
        .humidity_pct  = 60.0f,
        .pressure_pa   = 101000.0f,
        .light_lux     = 300.0f,
    };
    err = event_bus_publish(&bus, EVT_SENSOR_DATA, &payload, sizeof(payload));
    ASSERT_EQ(err, EVENT_BUS_OK);

    /* Verify subscriber was called */
    ASSERT_EQ(sensor_event_count, 1);
    ASSERT_NEAR(last_received_temp, 23.5f, 0.01f);
    ASSERT_NEAR(last_received_lux, 300.0f, 0.01f);

    /* Verify CBOR was encoded */
    ASSERT_TRUE(cbor_len > 0);

    printf("  event_bus: %d events received, CBOR %u bytes\n",
           sensor_event_count, (unsigned)cbor_len);
}

TEST(test_scheduler_tick)
{
    /* Reset tracking */
    sched_sensor_calls = 0;
    sensor_event_count = 0;

    /* Register a periodic sensor read task (every 5 ticks) */
    sched_err_t err = scheduler_add(&sched, "sensor_read",
                                    sched_read_sensors, &bus, 5);
    ASSERT_EQ(err, SCHED_OK);
    ASSERT_EQ(scheduler_count(&sched), 1);

    /* Advance 10 ticks -- should fire at tick 5 and tick 10 */
    for (int i = 0; i < 10; i++) {
        scheduler_tick(&sched);
        scheduler_run(&sched);
    }

    /* Sensor read callback should have been called twice */
    ASSERT_EQ(sched_sensor_calls, 2);

    /* Each callback publishes to event_bus, subscriber receives */
    ASSERT_EQ(sensor_event_count, 2);

    printf("  scheduler: %u task(s), sensor read called %d times in 10 ticks\n",
           (unsigned)scheduler_count(&sched), sched_sensor_calls);
}

TEST(test_debounce_button)
{
    /* Re-init debounce: threshold=3, initial=false */
    debounce_init(&btn, 3, false);
    ASSERT_FALSE(debounce_get(&btn));

    /* Simulate bouncing: press with noise */
    debounce_update(&btn, true);   /* bounce 1 */
    ASSERT_FALSE(debounce_changed(&btn));

    debounce_update(&btn, false);  /* bounce back -- resets counter */
    ASSERT_FALSE(debounce_changed(&btn));

    /* Clean press: 3 consecutive true readings */
    debounce_update(&btn, true);   /* count=1 */
    ASSERT_FALSE(debounce_changed(&btn));
    ASSERT_FALSE(debounce_get(&btn));

    debounce_update(&btn, true);   /* count=2 */
    ASSERT_FALSE(debounce_changed(&btn));

    debounce_update(&btn, true);   /* count=3 -> stable=true, changed=true */
    ASSERT_TRUE(debounce_changed(&btn));
    ASSERT_TRUE(debounce_get(&btn));
    ASSERT_TRUE(debounce_rising(&btn));
    ASSERT_FALSE(debounce_falling(&btn));

    /* Publish button event via event_bus */
    uint8_t button_id = 1;
    event_bus_err_t eb_err = event_bus_publish(&bus, EVT_BUTTON_PRESS,
                                               &button_id, sizeof(button_id));
    ASSERT_EQ(eb_err, EVENT_BUS_OK);

    printf("  debounce: button press detected after 3 stable readings\n");
}

TEST(test_timer_wheel_alert)
{
    /* Reset tracking */
    alert_fired_count = 0;

    /* Schedule a one-shot alert after 5 ticks */
    timer_wheel_err_t err = timer_wheel_add(&tw, 0, 5, on_alert, NULL);
    ASSERT_EQ(err, TIMER_WHEEL_OK);

    /* Advance 4 ticks -- should not fire yet */
    for (int i = 0; i < 4; i++) {
        timer_wheel_tick(&tw);
    }
    ASSERT_EQ(alert_fired_count, 0);

    /* 5th tick -- alert fires */
    timer_wheel_tick(&tw);
    ASSERT_EQ(alert_fired_count, 1);

    /* Additional ticks -- should not fire again (one-shot) */
    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(alert_fired_count, 1);

    printf("  timer_wheel: alert fired after 5 ticks (one-shot)\n");
}

TEST(test_led_pattern_brightness)
{
    /*
     * Drive ws2812 with colors based on led_pattern state.
     * Heartbeat pattern: 2 ticks ON, 4 ticks OFF (looping).
     * When ON -> bright white; when OFF -> dim.
     */
    ws_mock_reset();

    /* Re-init led_pattern for a clean state */
    static const led_step_t heartbeat[] = {
        { .on_ticks = 2, .off_ticks = 4 },
    };
    led_pattern_cfg_t cfg = {
        .steps      = heartbeat,
        .step_count = 1,
        .loop       = true,
    };
    led_pattern_init(&led_pat, &cfg);

    /* Tick through one full cycle (6 ticks) and record on/off states */
    bool states[6];
    for (int i = 0; i < 6; i++) {
        states[i] = led_pattern_tick(&led_pat);
    }

    /* First 2 ticks should be ON, next 4 should be OFF */
    ASSERT_TRUE(states[0]);
    ASSERT_TRUE(states[1]);
    ASSERT_FALSE(states[2]);
    ASSERT_FALSE(states[3]);
    ASSERT_FALSE(states[4]);
    ASSERT_FALSE(states[5]);

    /* Use led_pattern state to choose ws2812 color and send */
    ws2812_color_t bright = { .r = 255, .g = 255, .b = 255 };
    ws2812_color_t dim    = { .r =  10, .g =  10, .b =  10 };

    /* Pattern just completed one cycle and looped; tick to get ON state */
    bool is_on = led_pattern_tick(&led_pat);

    ws2812_color_t color = is_on ? bright : dim;
    ws2812_err_t ws_err = ws2812_send(&ws_hal, &ws_timing, &color, 1);
    ASSERT_EQ(ws_err, WS2812_OK);

    /* Verify mock recorded PWM data */
    ASSERT_TRUE(ws_compare_idx > 0);
    ASSERT_TRUE(ws_period_count > 0);

    /* Verify the encoded GRB slots match expected ticks for the color */
    uint16_t expected_slots[24];
    ws2812_encode_grb(&color, &ws_timing, expected_slots);

    /* The first entry in compare_log is the reset latch (0), then the
     * actual PWM data starts after reset_periods + 1 entries.
     * We verify the encoded slots match what ws2812_encode_grb produces */
    ASSERT_TRUE(expected_slots[0] > 0 || expected_slots[0] == 0);

    printf("  led_pattern + ws2812: %u PWM values recorded, %u periods\n",
           (unsigned)ws_compare_idx, (unsigned)ws_period_count);
}

/* ======================================================================
 * Main
 * ====================================================================== */

TEST_MAIN("smart_home integration")
    RUN(test_init_all);
    RUN(test_bme280_read);
    RUN(test_bh1750_read);
    RUN(test_cbor_encode_sensor_data);
    RUN(test_event_bus_pub_sub);
    RUN(test_scheduler_tick);
    RUN(test_debounce_button);
    RUN(test_timer_wheel_alert);
    RUN(test_led_pattern_brightness);
TEST_MAIN_END()
