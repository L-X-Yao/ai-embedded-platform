// SPDX-License-Identifier: MIT
/*
 * lowpower_sensor_node -- integration recipe: battery-powered sensor node
 *
 * Demonstrates six wheels working together:
 *   L1  max17048      -- LiPo battery fuel gauge (voltage + SOC)
 *   L1  ds3231        -- precision RTC (periodic wake source)
 *   L1  adxl345       -- accelerometer (motion wake source)
 *   L1  bme280        -- temperature / humidity / pressure
 *   L3  boot_counter  -- persistent wake counter (uniqueness proof)
 *   L3  power_manager -- cooperative sleep level orchestrator
 *
 * Scenario: a battery-powered IoT sensor node duty-cycles between deep
 * sleep and short wake bursts. It wakes from either an RTC tick or a
 * motion event, samples environmental + battery data, then re-enters
 * sleep. Low battery forces a higher-priority telemetry upload.
 *
 * Runs entirely on host with mock I2C -- no hardware required.
 * Build:  cmake -B build -S . && cmake --build build && ./build/lowpower_sensor_node
 */

#include "test_harness.h"

/* wheels */
#include "max17048.h"
#include "ds3231.h"
#include "adxl345.h"
#include "bme280.h"
#include "boot_counter.h"
#include "power_manager.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define WAKE_CYCLES            10    /* total wake/sleep cycles to simulate */
#define LOW_BATTERY_PCT      20.0f   /* SOC below this triggers emergency  */
#define MOTION_THRESHOLD_G    0.5f   /* |accel| - 1g must exceed this      */

/* Simulated time advance per cycle (ms) */
#define SLEEP_TICK_MS       30000U   /* 30 s between RTC wakes              */

/* Voter IDs into power_manager (assigned by register) */
static uint8_t voter_radio;
static uint8_t voter_sampler;

/* Simulated millisecond clock (advanced by the cycle loop) */
static uint32_t fake_ms;

static uint32_t fake_get_tick_ms(void *ctx)
{
    (void)ctx;
    return fake_ms;
}

/* ======================================================================
 * Mock I2C bus -- shared between max17048, ds3231, adxl345, bme280
 *
 * Each driver has its own dev_addr, so a single switch on dev_addr
 * routes register reads to the matching mock register file. This
 * mirrors how a real I2C bus serves multiple devices.
 * ====================================================================== */

static uint8_t  max_regs[64];   /* MAX17048 register file (16-bit BE words) */
static uint8_t  rtc_regs[32];   /* DS3231 register file (BCD time)          */
static uint8_t  adxl_regs[64];  /* ADXL345 register file                    */
static uint8_t  bme_regs[256];  /* BME280 register file                     */

/* Simulated motion magnitude (g) -- adxl_read derives from this */
static float    sim_motion_g = 0.0f;
/* Simulated battery state of charge (%) */
static float    sim_soc_pct  = 95.0f;
/* Simulated battery cell voltage (V) */
static float    sim_v_cell   = 4.05f;
/* Simulated temperature (deg C, used by BME280 mock) */
static float    sim_temp_c   = 22.5f;
/* Simulated humidity (%RH, used by BME280 mock) */
static float    sim_hum_pct  = 45.0f;

/* ---------- MAX17048 mock (16-bit BE registers at half-byte offset) ----- */

static void max_plant(void)
{
    /* VCELL register 0x02: 78.125 uV per LSB */
    uint16_t vcell = (uint16_t)(sim_v_cell / 78.125e-6f);
    max_regs[0x02] = (uint8_t)(vcell >> 8);
    max_regs[0x03] = (uint8_t)(vcell & 0xFF);

    /* SOC register 0x04: 1/256 % per LSB */
    uint16_t soc = (uint16_t)(sim_soc_pct * 256.0f);
    max_regs[0x04] = (uint8_t)(soc >> 8);
    max_regs[0x05] = (uint8_t)(soc & 0xFF);

    /* CRATE register 0x16: 0.208 %/hr per LSB (signed) */
    int16_t crate = -480;  /* ~ -1%/hr discharge */
    max_regs[0x16] = (uint8_t)((uint16_t)crate >> 8);
    max_regs[0x17] = (uint8_t)(crate & 0xFF);

    /* VERSION register 0x08: any value other than 0x0000 / 0xFFFF */
    max_regs[0x08] = 0x00;
    max_regs[0x09] = 0x12;
}

/* ---------- DS3231 mock (BCD time at registers 0x00..0x06) -------------- */

static uint8_t bcd_of(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

static void rtc_plant(uint8_t hh, uint8_t mm, uint8_t ss)
{
    rtc_regs[0x00] = bcd_of(ss);
    rtc_regs[0x01] = bcd_of(mm);
    rtc_regs[0x02] = bcd_of(hh);         /* 24-hour mode (bit 6 clear) */
    rtc_regs[0x03] = 0x03;               /* weekday = Wednesday */
    rtc_regs[0x04] = bcd_of(13);         /* day 13 */
    rtc_regs[0x05] = bcd_of(5);          /* month 5 */
    rtc_regs[0x06] = bcd_of(26);         /* year 2026 */

    /* Temperature register 0x11 (MSB) / 0x12 (fraction) */
    rtc_regs[0x11] = 23;                 /* 23 deg C */
    rtc_regs[0x12] = 0x40;               /* +0.25 deg C */
}

/* ---------- ADXL345 mock (chip ID @ 0x00, data @ 0x32..0x37) ------------ */

static void adxl_plant(void)
{
    adxl_regs[0x00] = 0xE5;              /* chip ID */

    /* full-resolution mode: 3.9 mg/LSB, so 1 g = 256 raw counts (approx) */
    /* X-axis carries the simulated motion magnitude */
    int16_t raw_x = (int16_t)(sim_motion_g / 0.0039f);
    int16_t raw_y = 0;
    /* Z axis = 1 g static gravity */
    int16_t raw_z = (int16_t)(1.0f / 0.0039f);

    adxl_regs[0x32] = (uint8_t)(raw_x & 0xFF);
    adxl_regs[0x33] = (uint8_t)((raw_x >> 8) & 0xFF);
    adxl_regs[0x34] = (uint8_t)(raw_y & 0xFF);
    adxl_regs[0x35] = (uint8_t)((raw_y >> 8) & 0xFF);
    adxl_regs[0x36] = (uint8_t)(raw_z & 0xFF);
    adxl_regs[0x37] = (uint8_t)((raw_z >> 8) & 0xFF);
}

/* ---------- BME280 mock (chip ID @ 0xD0; canned calibration + ADCs) ----- */

static void bme_plant(void)
{
    memset(bme_regs, 0, sizeof(bme_regs));
    bme_regs[0xD0] = 0x60;   /* chip ID */

    /* Realistic calibration values (from a real BME280) */
    bme_regs[0x88] = 0x70; bme_regs[0x89] = 0x6B;   /* dig_T1 = 27504 */
    bme_regs[0x8A] = 0x43; bme_regs[0x8B] = 0x67;   /* dig_T2 = 26435 */
    bme_regs[0x8C] = 0x18; bme_regs[0x8D] = 0xFC;   /* dig_T3 = -1000 */
    bme_regs[0x8E] = 0x7D; bme_regs[0x8F] = 0x8E;   /* dig_P1 = 36477 */
    bme_regs[0x90] = 0x43; bme_regs[0x91] = 0xD6;
    bme_regs[0x92] = 0xD0; bme_regs[0x93] = 0x0B;
    bme_regs[0x94] = 0x27; bme_regs[0x95] = 0x0B;
    bme_regs[0x96] = 0x8C; bme_regs[0x97] = 0x00;
    bme_regs[0x98] = 0xF9; bme_regs[0x99] = 0xFF;
    bme_regs[0x9A] = 0x8C; bme_regs[0x9B] = 0x3C;
    bme_regs[0x9C] = 0xF8; bme_regs[0x9D] = 0xC6;
    bme_regs[0x9E] = 0x70; bme_regs[0x9F] = 0x17;
    bme_regs[0xA1] = 0x4B;                          /* dig_H1 */

    /* H2-H6 at 0xE1..0xE7 (7 bytes) */
    bme_regs[0xE1] = 0x6D; bme_regs[0xE2] = 0x01;   /* dig_H2 = 365 */
    bme_regs[0xE3] = 0x00;                          /* dig_H3 = 0 */
    bme_regs[0xE4] = 0x12; bme_regs[0xE5] = 0x00;
    bme_regs[0xE6] = 0x1E; bme_regs[0xE7] = 0x18;

    /* ADC values yielding ~22 C / 50 %RH / 101300 Pa (close enough; the
     * exact value depends on the Bosch compensation math). For host
     * verification we only need plausible / non-zero readings. */
    uint32_t adc_p = 415148;
    uint32_t adc_t = 519888;
    uint16_t adc_h = (uint16_t)(sim_hum_pct * 600.0f);

    bme_regs[0xF7] = (uint8_t)(adc_p >> 12);
    bme_regs[0xF8] = (uint8_t)(adc_p >> 4);
    bme_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    bme_regs[0xFA] = (uint8_t)(adc_t >> 12);
    bme_regs[0xFB] = (uint8_t)(adc_t >> 4);
    bme_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
    bme_regs[0xFD] = (uint8_t)(adc_h >> 8);
    bme_regs[0xFE] = (uint8_t)(adc_h & 0xFF);

    /* Quiet -Wunused-but-set-variable for sim_temp_c -- we used sim_hum_pct
     * directly; sim_temp_c just documents simulated state for the printf. */
    (void)sim_temp_c;
}

/* ---------- Single I2C HAL routed by device address --------------------- */

static bool i2c_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx;
    /* No mock device actually depends on writes to settle; ignore. */
    (void)dev_addr; (void)reg; (void)val;
    return true;
}

/* The MAX17048 takes a write only as one word: register, MSB, LSB */
static bool max_write_raw(void *ctx, uint8_t dev_addr, const uint8_t *data,
                          uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (len == 3 && data[0] + 1u < sizeof(max_regs)) {
        max_regs[data[0]]      = data[1];
        max_regs[data[0] + 1u] = data[2];
    }
    return true;
}

static bool i2c_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len)
{
    (void)ctx;
    const uint8_t *src = NULL;
    size_t src_size = 0;

    switch (dev_addr) {
    case MAX17048_DEV_ADDR: src = max_regs;  src_size = sizeof(max_regs);  break;
    case DS3231_ADDR:       src = rtc_regs;  src_size = sizeof(rtc_regs);  break;
    case ADXL345_ADDR_PRIMARY: src = adxl_regs; src_size = sizeof(adxl_regs); break;
    case BME280_ADDR_LOW:   src = bme_regs;  src_size = sizeof(bme_regs);  break;
    default: return false;
    }

    for (uint16_t i = 0; i < len; i++) {
        uint16_t idx = (uint16_t)(reg + i);
        if (idx >= src_size) return false;
        buf[i] = src[idx];
    }
    return true;
}

static void i2c_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* HAL bundles for each driver */
static const max17048_i2c_t max_hal = {
    .read_regs = i2c_read_regs,
    .ctx       = NULL,
    .write_raw = max_write_raw,
};

static const ds3231_i2c_t rtc_hal = {
    .write_reg = i2c_write_reg,
    .read_regs = i2c_read_regs,
    .delay_ms  = i2c_delay_ms,
    .ctx       = NULL,
};

static const adxl345_i2c_t adxl_hal = {
    .write_reg = i2c_write_reg,
    .read_regs = i2c_read_regs,
    .delay_ms  = i2c_delay_ms,
    .ctx       = NULL,
};

static const bme280_i2c_t bme_hal = {
    .write_reg = i2c_write_reg,
    .read_regs = i2c_read_regs,
    .delay_ms  = i2c_delay_ms,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock NVM for boot_counter (16-byte EEPROM region)
 * ====================================================================== */

static uint8_t nvm_image[sizeof(boot_counter_record_t) + 8];
static bool    nvm_valid = true;

static int nvm_read(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (!nvm_valid) return -1;
    memcpy(buf, nvm_image, size);
    return 0;
}

static int nvm_write(const uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (size > sizeof(nvm_image)) return -1;
    memcpy(nvm_image, buf, size);
    nvm_valid = true;
    return 0;
}

static const boot_counter_storage_t nvm_storage = {
    .read     = nvm_read,
    .write    = nvm_write,
    .user_ctx = NULL,
};

/* ======================================================================
 * Global driver instances
 * ====================================================================== */

static max17048_t     fuel;
static ds3231_t       rtc;
static adxl345_t      accel;
static bme280_t       bme;
static boot_counter_t bc;
static power_manager_t pm;

/* Wake source classification */
typedef enum {
    WAKE_RTC      = 0,   /* periodic RTC tick                  */
    WAKE_MOTION   = 1,   /* accelerometer crossed threshold    */
    WAKE_EMERGENCY = 2,  /* low battery forced immediate wake  */
} wake_source_t;

/* ======================================================================
 * Tests -- integration recipe walk-through
 * ====================================================================== */

TEST(test_init_all_wheels)
{
    /* Plant fresh mock state */
    fake_ms = 0;
    nvm_valid = true;
    memset(nvm_image, 0, sizeof(nvm_image));
    max_plant();
    rtc_plant(8, 30, 0);
    adxl_plant();
    bme_plant();

    /* 1. max17048 -- battery fuel gauge */
    ASSERT_EQ(max17048_init(&fuel, &max_hal), MAX17048_OK);

    /* 2. ds3231 -- precision RTC */
    ASSERT_EQ(ds3231_init(&rtc, &rtc_hal), DS3231_OK);

    /* 3. adxl345 -- accelerometer (motion wake) */
    adxl345_cfg_t acfg = {
        .addr      = ADXL345_ADDR_PRIMARY,
        .range     = ADXL345_RANGE_2G,
        .data_rate = ADXL345_RATE_25HZ,
    };
    ASSERT_EQ(adxl345_init(&accel, &adxl_hal, &acfg), ADXL345_OK);

    /* 4. bme280 -- env data */
    bme280_cfg_t bcfg = {
        .addr   = BME280_ADDR_LOW,
        .osrs_t = BME280_OVERSAMPLING_1,
        .osrs_p = BME280_OVERSAMPLING_1,
        .osrs_h = BME280_OVERSAMPLING_1,
    };
    ASSERT_EQ(bme280_init(&bme, &bme_hal, &bcfg), BME280_OK);

    /* 5. boot_counter -- prove every wake increments persistently */
    ASSERT_EQ(boot_counter_init(&bc, &nvm_storage), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_load(&bc), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&bc), 0u);

    /* 6. power_manager -- cooperative sleep level voting */
    static const power_manager_io_t pio = {
        .get_tick_ms = fake_get_tick_ms,
        .ctx         = NULL,
    };
    ASSERT_EQ(power_manager_init(&pm, &pio), POWER_MANAGER_OK);

    /* Two voters: a radio (blocks deep sleep when uploading) and a sampler
     * (blocks light sleep while ADC conversion in flight). */
    ASSERT_EQ(power_manager_register(&pm, "radio",
                                     POWER_LEVEL_LIGHT_SLEEP,
                                     5000U, &voter_radio),
              POWER_MANAGER_OK);
    ASSERT_EQ(power_manager_register(&pm, "sampler",
                                     POWER_LEVEL_IDLE,
                                     1000U, &voter_sampler),
              POWER_MANAGER_OK);

    printf("    all 6 wheels initialized; 2 power voters registered\n");
}

TEST(test_battery_status_initial)
{
    /* Verify fuel gauge reports planted values */
    max17048_data_t batt;
    ASSERT_EQ(max17048_read(&fuel, &batt), MAX17048_OK);
    ASSERT_NEAR(batt.voltage_v, sim_v_cell, 0.01f);
    ASSERT_NEAR(batt.soc_pct,   sim_soc_pct, 0.5f);

    printf("    battery: %.2f V, %.1f %% SOC, %.2f %%/hr\n",
           (double)batt.voltage_v, (double)batt.soc_pct,
           (double)batt.charge_rate);
}

TEST(test_rtc_baseline)
{
    /* Confirm RTC reads back the planted time */
    ds3231_time_t now;
    ASSERT_EQ(ds3231_get_time(&rtc, &now), DS3231_OK);
    ASSERT_EQ(now.hour,   8);
    ASSERT_EQ(now.minute, 30);
    ASSERT_EQ(now.second, 0);

    printf("    RTC: 20%02u-%02u-%02u %02u:%02u:%02u\n",
           now.year, now.month, now.day,
           now.hour, now.minute, now.second);
}

/* Helper: classify wake source for a given cycle. Alternates RTC/motion;
 * cycle 4 forces an emergency low-battery wake. */
static wake_source_t classify_wake(uint8_t cycle, float soc_pct)
{
    if (soc_pct < LOW_BATTERY_PCT) return WAKE_EMERGENCY;
    return (cycle % 3 == 1) ? WAKE_MOTION : WAKE_RTC;
}

TEST(test_wake_cycle_loop)
{
    const char *src_name[] = { "RTC",      "motion",  "EMERG" };

    printf("\n  --- duty-cycling wake sequence ---\n");
    printf("  c | src    | bc# | t (ms) | SOC%%  | T  | motion |\n");
    printf("  --|--------|-----|--------|-------|----|--------|\n");

    uint32_t emergency_wakes = 0;
    uint32_t motion_wakes    = 0;
    uint32_t rtc_wakes       = 0;

    for (uint8_t cycle = 0; cycle < WAKE_CYCLES; cycle++) {
        /* Drain battery a bit each cycle so cycle 4 dips below threshold */
        sim_soc_pct -= 12.0f;
        if (sim_soc_pct < 5.0f) sim_soc_pct = 5.0f;
        sim_v_cell -= 0.05f;
        if (sim_v_cell < 3.30f) sim_v_cell = 3.30f;

        /* Simulate motion on every 3rd cycle */
        sim_motion_g = (cycle % 3 == 1) ? 1.2f : 0.05f;

        /* Re-plant mock register files (post-sleep wake) */
        max_plant();
        adxl_plant();
        bme_plant();
        rtc_plant((uint8_t)(8 + (cycle / 2)),
                  (uint8_t)((30 + cycle * 5) % 60), 0);

        /* Advance the simulated clock as if we slept */
        fake_ms += SLEEP_TICK_MS;

        /* ====== WAKE: read battery first to decide priority ====== */
        max17048_data_t batt = (max17048_data_t){0};
        ASSERT_EQ(max17048_read(&fuel, &batt), MAX17048_OK);

        wake_source_t src = classify_wake(cycle, batt.soc_pct);
        if (src == WAKE_EMERGENCY) emergency_wakes++;
        else if (src == WAKE_MOTION) motion_wakes++;
        else rtc_wakes++;

        /* Read accelerometer (validates motion magnitude during MOTION wake) */
        adxl345_data_t acc;
        ASSERT_EQ(adxl345_read(&accel, &acc), ADXL345_OK);
        float net_g = acc.x_g;  /* sign matters less than magnitude */
        if (src == WAKE_MOTION) {
            ASSERT_TRUE(net_g > MOTION_THRESHOLD_G);
        }

        /* Read BME280 (always sampled during a wake) */
        bme280_data_t env;
        ASSERT_EQ(bme280_read(&bme, &env), BME280_OK);

        /* Read RTC (always; tells us how long we slept) */
        ds3231_time_t now;
        ASSERT_EQ(ds3231_get_time(&rtc, &now), DS3231_OK);

        /* Tag boot/wake count (per-wake increment proves uniqueness) */
        ASSERT_EQ(boot_counter_increment(&bc), BOOT_COUNTER_OK);

        /* ====== ACTIVE phase: tell power_manager we are busy ====== */
        ASSERT_EQ(power_manager_activity(&pm, voter_sampler), POWER_MANAGER_OK);

        /* Emergency wakes also engage the radio (block deep sleep harder) */
        if (src == WAKE_EMERGENCY) {
            ASSERT_EQ(power_manager_activity(&pm, voter_radio), POWER_MANAGER_OK);
            /* Active level should NOT permit deep sleep while radio busy */
            ASSERT_LT(power_manager_poll(&pm), POWER_LEVEL_DEEP_SLEEP);
        }

        printf("  %u | %-6s | %3u | %6u | %5.1f | %2u | %5.2fg |\n",
               (unsigned)cycle, src_name[src],
               (unsigned)boot_counter_get_count(&bc),
               (unsigned)fake_ms, (double)batt.soc_pct,
               now.hour, (double)net_g);

        /* Suppress -Wunused for env on builds with extreme settings */
        (void)env;

        /* ====== SLEEP phase: release voters; poll deepest allowed ===== */
        ASSERT_EQ(power_manager_release(&pm, voter_sampler), POWER_MANAGER_OK);
        ASSERT_EQ(power_manager_release(&pm, voter_radio),   POWER_MANAGER_OK);

        power_level_t lvl = power_manager_poll(&pm);
        /* With no voter active, deepest level (SHUTDOWN) is allowed */
        ASSERT_EQ(lvl, POWER_LEVEL_SHUTDOWN);
    }

    printf("  --|--------|-----|--------|-------|----|--------|\n");
    printf("  totals: rtc=%u motion=%u emergency=%u (sum=%u, expected %u)\n",
           (unsigned)rtc_wakes, (unsigned)motion_wakes,
           (unsigned)emergency_wakes,
           (unsigned)(rtc_wakes + motion_wakes + emergency_wakes),
           (unsigned)WAKE_CYCLES);

    ASSERT_EQ(rtc_wakes + motion_wakes + emergency_wakes,
              (uint32_t)WAKE_CYCLES);

    /* At least one emergency wake must have fired once the battery
     * dropped below LOW_BATTERY_PCT */
    ASSERT_GE(emergency_wakes, 1);
    ASSERT_GE(motion_wakes,   1);
}

TEST(test_boot_counter_persisted)
{
    /* After WAKE_CYCLES wake events the counter must equal WAKE_CYCLES */
    ASSERT_EQ(boot_counter_get_count(&bc), (uint32_t)WAKE_CYCLES);

    /* Simulate a power-cycle: fresh boot_counter must reload the value */
    boot_counter_t bc_reboot;
    ASSERT_EQ(boot_counter_init(&bc_reboot, &nvm_storage), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_load(&bc_reboot), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&bc_reboot), (uint32_t)WAKE_CYCLES);

    printf("    boot_counter survived simulated reboot: %u wakes\n",
           (unsigned)boot_counter_get_count(&bc_reboot));
}

TEST(test_power_manager_voter_lockout)
{
    /* Confirm that an active voter prevents deep sleep */
    ASSERT_EQ(power_manager_activity(&pm, voter_sampler), POWER_MANAGER_OK);
    power_level_t lvl = power_manager_poll(&pm);
    ASSERT_LE(lvl, POWER_LEVEL_IDLE);   /* sampler caps at IDLE */

    /* Now arm the radio voter -- caps at LIGHT_SLEEP, so the result is
     * the more restrictive of the two = IDLE */
    ASSERT_EQ(power_manager_activity(&pm, voter_radio), POWER_MANAGER_OK);
    lvl = power_manager_poll(&pm);
    ASSERT_EQ(lvl, POWER_LEVEL_IDLE);

    /* Release sampler: radio remains, so level rises to LIGHT_SLEEP */
    ASSERT_EQ(power_manager_release(&pm, voter_sampler), POWER_MANAGER_OK);
    lvl = power_manager_poll(&pm);
    ASSERT_EQ(lvl, POWER_LEVEL_LIGHT_SLEEP);

    /* Release everything -> SHUTDOWN permissible */
    ASSERT_EQ(power_manager_release(&pm, voter_radio), POWER_MANAGER_OK);
    lvl = power_manager_poll(&pm);
    ASSERT_EQ(lvl, POWER_LEVEL_SHUTDOWN);

    printf("    voter lockout chain: sampler(IDLE) + radio(LIGHT_SLEEP) "
           "-> recommended level falls correctly\n");
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("lowpower_sensor_node integration")
    RUN(test_init_all_wheels);
    RUN(test_battery_status_initial);
    RUN(test_rtc_baseline);
    RUN(test_wake_cycle_loop);
    RUN(test_boot_counter_persisted);
    RUN(test_power_manager_voter_lockout);
TEST_MAIN_END()
