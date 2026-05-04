// SPDX-License-Identifier: MIT
/*
 * env_monitor -- integration recipe: environmental monitoring
 *
 * Demonstrates five wheels working together:
 *   L0  ring_buffer    -- backing store for data_logger
 *   L1  bmp280         -- temperature + pressure sensor
 *   L1  sht30          -- temperature + humidity sensor
 *   L3  sensor_hub     -- multi-sensor polling + statistics
 *   L3  data_logger    -- CSV record logging
 *
 * Runs entirely on host with mock I2C -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/env_monitor
 */

#include "test_harness.h"

/* wheels */
#include "ring_buffer.h"
#include "bmp280.h"
#include "sht30.h"
#include "sensor_hub.h"
#include "data_logger.h"

#include <string.h>
#include <math.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define NUM_READINGS     10   /* number of poll cycles to run     */
#define LOG_STORE_SIZE 2048   /* ring_buffer backing store bytes  */

/* ======================================================================
 * Mock I2C -- BMP280 (register-based: write_reg / read_regs)
 * ====================================================================== */

static uint8_t bmp_regs[256];
static int     bmp_op_count;

static void bmp_mock_reset(void)
{
    memset(bmp_regs, 0, sizeof(bmp_regs));
    bmp_op_count = 0;

    /* Chip ID */
    bmp_regs[0xD0] = 0x58;

    /* Calibration data (realistic values from a real BMP280) */
    bmp_regs[0x88] = 0x70; bmp_regs[0x89] = 0x6B;  /* dig_T1 = 27504 */
    bmp_regs[0x8A] = 0x43; bmp_regs[0x8B] = 0x67;  /* dig_T2 = 26435 */
    bmp_regs[0x8C] = 0x18; bmp_regs[0x8D] = 0xFC;  /* dig_T3 = -1000 */
    bmp_regs[0x8E] = 0x7D; bmp_regs[0x8F] = 0x8E;  /* dig_P1 = 36477 */
    bmp_regs[0x90] = 0x43; bmp_regs[0x91] = 0xD6;  /* dig_P2 */
    bmp_regs[0x92] = 0xD0; bmp_regs[0x93] = 0x0B;  /* dig_P3 */
    bmp_regs[0x94] = 0x27; bmp_regs[0x95] = 0x0B;  /* dig_P4 */
    bmp_regs[0x96] = 0x8C; bmp_regs[0x97] = 0x00;  /* dig_P5 */
    bmp_regs[0x98] = 0xF9; bmp_regs[0x99] = 0xFF;  /* dig_P6 */
    bmp_regs[0x9A] = 0x8C; bmp_regs[0x9B] = 0x3C;  /* dig_P7 */
    bmp_regs[0x9C] = 0xF8; bmp_regs[0x9D] = 0xC6;  /* dig_P8 */
    bmp_regs[0x9E] = 0x70; bmp_regs[0x9F] = 0x17;  /* dig_P9 */

    /* STATUS register: measuring=0 (conversion ready) */
    bmp_regs[0xF3] = 0x00;

    /* Plant ADC values: adc_t=519888 (~25C), adc_p=415148 (~101325 Pa) */
    uint32_t adc_p = 415148;
    uint32_t adc_t = 519888;
    bmp_regs[0xF7] = (uint8_t)(adc_p >> 12);
    bmp_regs[0xF8] = (uint8_t)(adc_p >> 4);
    bmp_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    bmp_regs[0xFA] = (uint8_t)(adc_t >> 12);
    bmp_regs[0xFB] = (uint8_t)(adc_t >> 4);
    bmp_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
}

static bool bmp_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    bmp_op_count++;
    bmp_regs[reg] = val;
    return true;
}

static bool bmp_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    bmp_op_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = bmp_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const bmp280_i2c_t bmp_i2c = {
    .write_reg = bmp_write_reg,
    .read_regs = bmp_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock I2C -- SHT30 (raw buffer: write / read)
 * ====================================================================== */

static uint8_t sht_read_buf[6];
static int     sht_op_count;

static void sht_plant_reading(float temp_c, float rh_pct)
{
    /* Convert back to raw values using SHT30 formulas:
     *   temp_c = -45 + 175 * raw_t / 65535
     *   rh_pct = 100 * raw_h / 65535
     */
    uint16_t raw_t = (uint16_t)(((temp_c + 45.0f) / 175.0f) * 65535.0f);
    uint16_t raw_h = (uint16_t)((rh_pct / 100.0f) * 65535.0f);

    sht_read_buf[0] = (uint8_t)(raw_t >> 8);
    sht_read_buf[1] = (uint8_t)(raw_t & 0xFF);
    sht_read_buf[2] = sht30_crc8(&sht_read_buf[0], 2);
    sht_read_buf[3] = (uint8_t)(raw_h >> 8);
    sht_read_buf[4] = (uint8_t)(raw_h & 0xFF);
    sht_read_buf[5] = sht30_crc8(&sht_read_buf[3], 2);
}

static void sht_mock_reset(void)
{
    memset(sht_read_buf, 0, sizeof(sht_read_buf));
    sht_op_count = 0;
    sht_plant_reading(25.0f, 50.0f);
}

static bool sht_write(void *ctx, uint8_t dev_addr, const uint8_t *data,
                       uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)data; (void)len;
    sht_op_count++;
    return true;
}

static bool sht_read(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    sht_op_count++;
    for (uint16_t i = 0; i < len && i < sizeof(sht_read_buf); i++) {
        data[i] = sht_read_buf[i];
    }
    return true;
}

static const sht30_i2c_t sht_i2c = {
    .write    = sht_write,
    .read     = sht_read,
    .delay_ms = NULL,
    .ctx      = NULL,
};

/* ======================================================================
 * Sensor-hub adapters
 *
 * sensor_hub expects: sensor_hub_err_t (*read)(void *ctx, float *value)
 * Each adapter bridges a driver's read API to the hub callback signature.
 * ====================================================================== */

/* BMP280 adapter: reads temperature */
static sensor_hub_err_t bmp_temp_adapter(void *ctx, float *value)
{
    bmp280_t *dev = (bmp280_t *)ctx;
    bmp280_data_t data;
    bmp280_err_t err = bmp280_read(dev, &data);
    if (err != BMP280_OK) {
        return SENSOR_HUB_ERR_READ;
    }
    *value = data.temperature_c;
    return SENSOR_HUB_OK;
}

/* BMP280 adapter: reads pressure */
static sensor_hub_err_t bmp_press_adapter(void *ctx, float *value)
{
    bmp280_t *dev = (bmp280_t *)ctx;
    bmp280_data_t data;
    bmp280_err_t err = bmp280_read(dev, &data);
    if (err != BMP280_OK) {
        return SENSOR_HUB_ERR_READ;
    }
    *value = data.pressure_pa;
    return SENSOR_HUB_OK;
}

/* SHT30 adapter: reads temperature */
static sensor_hub_err_t sht_temp_adapter(void *ctx, float *value)
{
    sht30_t *dev = (sht30_t *)ctx;
    sht30_data_t data;
    sht30_err_t err = sht30_read(dev, &data);
    if (err != SHT30_OK) {
        return SENSOR_HUB_ERR_READ;
    }
    *value = data.temperature_c;
    return SENSOR_HUB_OK;
}

/* SHT30 adapter: reads humidity */
static sensor_hub_err_t sht_humid_adapter(void *ctx, float *value)
{
    sht30_t *dev = (sht30_t *)ctx;
    sht30_data_t data;
    sht30_err_t err = sht30_read(dev, &data);
    if (err != SHT30_OK) {
        return SENSOR_HUB_ERR_READ;
    }
    *value = data.humidity_pct;
    return SENSOR_HUB_OK;
}

/* ======================================================================
 * Data logger storage backend -- backed by ring_buffer
 *
 * data_logger writes CSV records; this backend stores them in a
 * ring_buffer so the application can read them back later.
 * ====================================================================== */

static ring_buffer_t  log_rb;
static uint8_t        log_store[LOG_STORE_SIZE];

static data_logger_err_t rb_write(void *ctx, const uint8_t *data, uint16_t len)
{
    ring_buffer_t *rb = (ring_buffer_t *)ctx;
    uint16_t written = ring_buffer_write(rb, data, len);
    if (written < len) {
        return DATA_LOGGER_ERR_OVERFLOW;
    }
    return DATA_LOGGER_OK;
}

static data_logger_err_t rb_flush(void *ctx)
{
    (void)ctx;
    return DATA_LOGGER_OK;
}

static const data_logger_storage_t rb_storage = {
    .write = rb_write,
    .flush = rb_flush,
    .ctx   = &log_rb,
};

/* ======================================================================
 * Global instances
 * ====================================================================== */

static bmp280_t     bmp_dev;
static sht30_t      sht_dev;
static sensor_hub_t hub;
static data_logger_t logger;

/* Field schema for the CSV log:
 *   timestamp, bmp_temp, bmp_press, sht_temp, sht_humid
 */
static const data_logger_field_t log_fields[] = {
    { "bmp_temp_c",    DATA_LOGGER_FIELD_FLOAT },
    { "bmp_press_pa",  DATA_LOGGER_FIELD_FLOAT },
    { "sht_temp_c",    DATA_LOGGER_FIELD_FLOAT },
    { "sht_humid_pct", DATA_LOGGER_FIELD_FLOAT },
};
#define LOG_FIELD_COUNT  (sizeof(log_fields) / sizeof(log_fields[0]))

/* ======================================================================
 * Tests -- verify the integration works end-to-end
 * ====================================================================== */

TEST(test_init_all_wheels)
{
    /* 1. ring_buffer */
    ring_buffer_err_t rb_err = ring_buffer_init(&log_rb, log_store,
                                                 LOG_STORE_SIZE);
    ASSERT_EQ(rb_err, RING_BUFFER_OK);
    ASSERT_TRUE(ring_buffer_is_empty(&log_rb));

    /* 2. bmp280 */
    bmp_mock_reset();
    bmp280_cfg_t bmp_cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_1,
        .osrs_p = BMP280_OVERSAMPLING_1,
    };
    bmp280_err_t bmp_err = bmp280_init(&bmp_dev, &bmp_i2c, &bmp_cfg);
    ASSERT_EQ(bmp_err, BMP280_OK);

    /* 3. sht30 */
    sht_mock_reset();
    sht30_cfg_t sht_cfg = { .addr = SHT30_ADDR_A };
    sht30_err_t sht_err = sht30_init(&sht_dev, &sht_i2c, &sht_cfg);
    ASSERT_EQ(sht_err, SHT30_OK);

    /* 4. sensor_hub -- register all four channels */
    sensor_hub_err_t hub_err = sensor_hub_init(&hub);
    ASSERT_EQ(hub_err, SENSOR_HUB_OK);

    hub_err = sensor_hub_register(&hub, "bmp_temp", "C",
                                  bmp_temp_adapter, &bmp_dev);
    ASSERT_EQ(hub_err, SENSOR_HUB_OK);

    hub_err = sensor_hub_register(&hub, "bmp_press", "Pa",
                                  bmp_press_adapter, &bmp_dev);
    ASSERT_EQ(hub_err, SENSOR_HUB_OK);

    hub_err = sensor_hub_register(&hub, "sht_temp", "C",
                                  sht_temp_adapter, &sht_dev);
    ASSERT_EQ(hub_err, SENSOR_HUB_OK);

    hub_err = sensor_hub_register(&hub, "sht_humid", "%RH",
                                  sht_humid_adapter, &sht_dev);
    ASSERT_EQ(hub_err, SENSOR_HUB_OK);

    ASSERT_EQ(sensor_hub_count(&hub), 4);

    /* 5. data_logger -- wired to ring_buffer backend */
    data_logger_err_t dl_err = data_logger_init(&logger, &rb_storage,
                                                 log_fields,
                                                 (uint8_t)LOG_FIELD_COUNT);
    ASSERT_EQ(dl_err, DATA_LOGGER_OK);
}

TEST(test_sensor_hub_find)
{
    /* Verify we can find sensors by name */
    ASSERT_EQ(sensor_hub_find(&hub, "bmp_temp"), 0);
    ASSERT_EQ(sensor_hub_find(&hub, "bmp_press"), 1);
    ASSERT_EQ(sensor_hub_find(&hub, "sht_temp"), 2);
    ASSERT_EQ(sensor_hub_find(&hub, "sht_humid"), 3);
    ASSERT_EQ(sensor_hub_find(&hub, "nonexistent"), -1);
}

TEST(test_single_poll_cycle)
{
    /* Poll all sensors once */
    sensor_hub_err_t err = sensor_hub_poll(&hub);
    ASSERT_EQ(err, SENSOR_HUB_OK);

    /* Verify BMP280 temperature is in plausible range */
    float val = 0.0f;
    sensor_hub_status_t status;
    sensor_hub_get(&hub, 0, &val, &status);
    ASSERT_EQ(status, SENSOR_HUB_STATUS_OK);
    ASSERT_TRUE(val > 15.0f && val < 45.0f);

    /* Verify BMP280 pressure is plausible */
    sensor_hub_get(&hub, 1, &val, &status);
    ASSERT_EQ(status, SENSOR_HUB_STATUS_OK);
    ASSERT_TRUE(val > 30000.0f && val < 110000.0f);

    /* Verify SHT30 temperature (~25C) */
    sensor_hub_get(&hub, 2, &val, &status);
    ASSERT_EQ(status, SENSOR_HUB_STATUS_OK);
    ASSERT_NEAR(val, 25.0f, 0.5f);

    /* Verify SHT30 humidity (~50%) */
    sensor_hub_get(&hub, 3, &val, &status);
    ASSERT_EQ(status, SENSOR_HUB_STATUS_OK);
    ASSERT_NEAR(val, 50.0f, 0.5f);
}

TEST(test_poll_and_log_n_readings)
{
    /* Reset the ring buffer and logger for a clean run */
    ring_buffer_reset(&log_rb);

    printf("\n  --- Simulated Environmental Readings ---\n");
    printf("  tick  bmp_temp   bmp_press  sht_temp  sht_humid\n");
    printf("  ----  ---------  ---------  --------  ---------\n");

    for (uint32_t tick = 0; tick < NUM_READINGS; tick++) {
        /* Vary the SHT30 mock slightly each tick for realism */
        float sim_temp = 24.0f + (float)(tick % 5) * 0.3f;
        float sim_rh   = 48.0f + (float)(tick % 4) * 1.0f;
        sht_plant_reading(sim_temp, sim_rh);

        /* Poll all sensors */
        sensor_hub_err_t err = sensor_hub_poll(&hub);
        ASSERT_EQ(err, SENSOR_HUB_OK);

        /* Read values from hub */
        float bmp_t = 0.0f, bmp_p = 0.0f, sht_t = 0.0f, sht_h = 0.0f;
        sensor_hub_get(&hub, 0, &bmp_t, NULL);
        sensor_hub_get(&hub, 1, &bmp_p, NULL);
        sensor_hub_get(&hub, 2, &sht_t, NULL);
        sensor_hub_get(&hub, 3, &sht_h, NULL);

        printf("  %4u  %7.2f C  %9.0f  %6.2f C  %6.2f %%\n",
               (unsigned)tick, (double)bmp_t, (double)bmp_p,
               (double)sht_t, (double)sht_h);

        /* Build a data_logger record and commit to ring_buffer */
        data_logger_err_t dl_err = data_logger_begin(&logger, tick);
        ASSERT_EQ(dl_err, DATA_LOGGER_OK);

        dl_err = data_logger_add_float(&logger, bmp_t);
        ASSERT_EQ(dl_err, DATA_LOGGER_OK);

        dl_err = data_logger_add_float(&logger, bmp_p);
        ASSERT_EQ(dl_err, DATA_LOGGER_OK);

        dl_err = data_logger_add_float(&logger, sht_t);
        ASSERT_EQ(dl_err, DATA_LOGGER_OK);

        dl_err = data_logger_add_float(&logger, sht_h);
        ASSERT_EQ(dl_err, DATA_LOGGER_OK);

        dl_err = data_logger_commit(&logger);
        ASSERT_EQ(dl_err, DATA_LOGGER_OK);
    }

    /* Verify record count */
    ASSERT_EQ(data_logger_get_count(&logger), NUM_READINGS);

    /* Verify ring_buffer has data */
    ASSERT_FALSE(ring_buffer_is_empty(&log_rb));
    printf("  ---\n");
    printf("  Logged %u records, ring_buffer holds %u bytes\n",
           (unsigned)data_logger_get_count(&logger),
           (unsigned)ring_buffer_count(&log_rb));
}

TEST(test_read_back_csv_from_ring_buffer)
{
    /* Read all stored CSV data from the ring_buffer */
    uint16_t stored = ring_buffer_count(&log_rb);
    ASSERT_TRUE(stored > 0);

    uint8_t readback[LOG_STORE_SIZE];
    uint16_t got = ring_buffer_read(&log_rb, readback, stored);
    ASSERT_EQ(got, stored);

    /* Null-terminate for printing */
    readback[got] = '\0';

    printf("\n  --- CSV Data from ring_buffer ---\n");
    printf("  %s", (char *)readback);

    /* Verify we can find the expected number of newlines (one per record) */
    int newlines = 0;
    for (uint16_t i = 0; i < got; i++) {
        if (readback[i] == '\n') {
            newlines++;
        }
    }
    ASSERT_EQ(newlines, NUM_READINGS);

    /* Verify ring_buffer is now empty after reading */
    ASSERT_TRUE(ring_buffer_is_empty(&log_rb));
}

TEST(test_sensor_hub_statistics)
{
    /* After NUM_READINGS + 1 polls (1 from test_single_poll_cycle +
     * NUM_READINGS from test_poll_and_log), check stats */
    float min_val = 0.0f, max_val = 0.0f, avg_val = 0.0f;

    /* SHT30 temperature stats: varied from 24.0 to 25.2 */
    sensor_hub_get_stats(&hub, 2, &min_val, &max_val, &avg_val);
    ASSERT_TRUE(min_val < max_val);
    ASSERT_TRUE(avg_val >= min_val && avg_val <= max_val);

    /* SHT30 humidity stats: varied from 48.0 to 51.0 */
    sensor_hub_get_stats(&hub, 3, &min_val, &max_val, &avg_val);
    ASSERT_TRUE(min_val < max_val);
    ASSERT_TRUE(avg_val >= min_val && avg_val <= max_val);

    printf("\n  --- Sensor Hub Statistics ---\n");
    for (uint8_t i = 0; i < sensor_hub_count(&hub); i++) {
        float mn = 0.0f, mx = 0.0f, av = 0.0f;
        sensor_hub_get_stats(&hub, i, &mn, &mx, &av);
        float val = 0.0f;
        sensor_hub_get(&hub, i, &val, NULL);
        printf("  [%u] last=%.2f  min=%.2f  max=%.2f  avg=%.2f\n",
               (unsigned)i, (double)val, (double)mn, (double)mx, (double)av);
    }
}

TEST(test_stat_reset)
{
    sensor_hub_reset_stats(&hub);

    /* After reset, do one more poll and verify count=1 equivalent */
    sensor_hub_poll(&hub);

    float min_val = 0.0f, max_val = 0.0f, avg_val = 0.0f;
    sensor_hub_get_stats(&hub, 0, &min_val, &max_val, &avg_val);
    /* After one reading, min == max == avg */
    ASSERT_NEAR(min_val, max_val, 0.001f);
    ASSERT_NEAR(min_val, avg_val, 0.001f);
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("env_monitor integration")
    RUN(test_init_all_wheels);
    RUN(test_sensor_hub_find);
    RUN(test_single_poll_cycle);
    RUN(test_poll_and_log_n_readings);
    RUN(test_read_back_csv_from_ring_buffer);
    RUN(test_sensor_hub_statistics);
    RUN(test_stat_reset);
TEST_MAIN_END()
