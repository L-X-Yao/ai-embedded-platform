// SPDX-License-Identifier: MIT
/*
 * power_monitor -- integration recipe: battery power monitoring with alerts
 *
 * Demonstrates eleven wheels working together:
 *   L0  bitmap          -- status flags bitfield (charging, low_battery, ...)
 *   L0  memory_pool     -- fixed-size block allocator for log entries
 *   L0  timeout         -- non-blocking measurement interval tracking
 *   L0  endian          -- big-endian I2C register byte-order conversion
 *   L1  max17048        -- fuel gauge (battery SoC / voltage)
 *   L1  ina226          -- precision current/voltage monitor
 *   L1  gpio_button     -- button with debounce
 *   L2  i2c_scan        -- I2C bus device discovery
 *   L3  power_manager   -- cooperative power state machine
 *   L3  button_handler  -- multi-button event handler
 *   L3  ring_log        -- circular log buffer
 *
 * Runs entirely on host with mock I2C + GPIO -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/power_monitor
 */

#include "test_harness.h"

/* L0 primitives */
#include "bitmap.h"
#include "memory_pool.h"
#include "timeout.h"
#include "endian.h"

/* L1 drivers */
#include "max17048.h"
#include "ina226.h"
#include "gpio_button.h"

/* L2 protocols */
#include "i2c_scan.h"

/* L3 modules */
#include "power_manager.h"
#include "button_handler.h"
#include "ring_log.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define MEASUREMENT_INTERVAL_MS  1000u
#define POOL_BLOCK_SIZE            64u
#define POOL_BLOCK_COUNT            8u
#define TICK_PERIOD_MS             10u

/* Status flag bit indices */
#define FLAG_CHARGING        0u
#define FLAG_LOW_BATTERY     1u
#define FLAG_OVERCURRENT     2u
#define FLAG_FULLY_CHARGED   3u
#define FLAG_SENSOR_FAULT    4u
#define FLAG_SLEEP_REQUEST   5u
#define FLAG_ALERT_ACTIVE    6u
#define FLAG_BUTTON_EVENT    7u

/* ======================================================================
 * Mock I2C -- shared register-based backend
 *
 * MAX17048 at 0x36: 16-bit big-endian registers
 * INA226  at 0x40: 16-bit big-endian registers
 * ====================================================================== */

/* MAX17048 register space */
static uint8_t max_regs[256];

static void max_mock_reset(void)
{
    memset(max_regs, 0, sizeof(max_regs));

    /* VERSION register (0x08-0x09): non-zero, non-FFFF = valid chip */
    max_regs[0x08] = 0x00;
    max_regs[0x09] = 0x11;

    /* VCELL register (0x02-0x03): ~3.8V = 3800mV / 78.125uV = 48640 = 0xBE00 */
    uint16_t vcell = 48640u;
    max_regs[0x02] = (uint8_t)(vcell >> 8);
    max_regs[0x03] = (uint8_t)(vcell & 0xFF);

    /* SOC register (0x04-0x05): 75.5% = 75.5 * 256 = 19328 = 0x4B80 */
    uint16_t soc = 19328u;
    max_regs[0x04] = (uint8_t)(soc >> 8);
    max_regs[0x05] = (uint8_t)(soc & 0xFF);

    /* CRATE register (0x16-0x17): +2.0 %/hr = 2.0 / 0.208 ~= 10 = 0x000A */
    uint16_t crate = 10u;
    max_regs[0x16] = (uint8_t)(crate >> 8);
    max_regs[0x17] = (uint8_t)(crate & 0xFF);

    /* CONFIG register (0x0C-0x0D): default */
    max_regs[0x0C] = 0x97;
    max_regs[0x0D] = 0x1C;
}

static bool max_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    max_regs[reg] = val;
    return true;
}

static bool max_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = max_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const max17048_i2c_t max_i2c = {
    .write_reg = max_write_reg,
    .read_regs = max_read_regs,
    .ctx       = NULL,
};

/* INA226 register space */
static uint8_t ina_regs[512]; /* 256 registers x 2 bytes, indexed by reg*2 */

static void ina_plant_reg(uint8_t reg, uint16_t val)
{
    ina_regs[reg * 2u]     = (uint8_t)(val >> 8);
    ina_regs[reg * 2u + 1] = (uint8_t)(val & 0xFF);
}

static void ina_mock_reset(void)
{
    memset(ina_regs, 0, sizeof(ina_regs));

    /* Manufacturer ID: 0x5449 */
    ina_plant_reg(0xFE, INA226_MFR_ID);
    /* Die ID: 0x2260 */
    ina_plant_reg(0xFF, INA226_DIE_ID);

    /* Shunt voltage register (0x01): 4000 * 2.5uV = 10mV -> 200mA at 50mohm */
    ina_plant_reg(0x01, 4000u);

    /* Bus voltage register (0x02): 12000 * 1.25mV = 15000mV = 15V */
    ina_plant_reg(0x02, 12000u);

    /* Current register (0x04): set after calibration */
    ina_plant_reg(0x04, 500u);

    /* Power register (0x03): set after calibration */
    ina_plant_reg(0x03, 100u);
}

static bool ina_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    /* INA226 reads 2 bytes per register */
    buf[0] = ina_regs[reg * 2u];
    if (len > 1) {
        buf[1] = ina_regs[reg * 2u + 1];
    }
    return true;
}

static bool ina_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                            const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ina_regs[reg * 2u] = buf[0];
    if (len > 1) {
        ina_regs[reg * 2u + 1] = buf[1];
    }
    return true;
}

static const ina226_i2c_t ina_i2c = {
    .read_regs  = ina_read_regs,
    .write_regs = ina_write_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

/* ======================================================================
 * Mock I2C probe for i2c_scan
 *
 * Only ACK addresses 0x36 (MAX17048) and 0x40 (INA226).
 * ====================================================================== */

static bool mock_probe(void *ctx, uint8_t addr)
{
    (void)ctx;
    return (addr == MAX17048_DEV_ADDR) || (addr == INA226_ADDR_DEFAULT);
}

/* ======================================================================
 * Mock GPIO for button
 * ====================================================================== */

static uint8_t mock_button_state = 0; /* 0 = released, 1 = pressed */

static uint8_t mock_gpio_read(void *ctx)
{
    (void)ctx;
    return mock_button_state;
}

static const gpio_button_hal_t btn_hal = {
    .read = mock_gpio_read,
    .ctx  = NULL,
};

/* ======================================================================
 * Mock GPIO read for button_handler (bool return type)
 * ====================================================================== */

static bool mock_btn_read(void *ctx)
{
    (void)ctx;
    return mock_button_state != 0;
}

/* ======================================================================
 * Mock tick for power_manager
 * ====================================================================== */

static uint32_t mock_tick_ms = 0;

static uint32_t mock_get_tick(void *ctx)
{
    (void)ctx;
    return mock_tick_ms;
}

static const power_manager_io_t pm_io = {
    .get_tick_ms = mock_get_tick,
    .ctx         = NULL,
};

/* ======================================================================
 * Button handler callback -- records events to ring_log
 * ====================================================================== */

static ring_log_t sys_log;

static void btn_event_callback(void *ctx, uint8_t button_id, btn_event_t event)
{
    (void)ctx;
    const char *names[] = {
        "NONE", "PRESS", "RELEASE", "CLICK",
        "DOUBLE_CLICK", "LONG_PRESS", "REPEAT"
    };
    const char *name = (event < 7) ? names[event] : "UNKNOWN";
    ring_log_write(&sys_log, RING_LOG_INFO, mock_tick_ms,
                   "BTN%u: %s", (unsigned)button_id, name);
}

/* ======================================================================
 * Global instances
 * ====================================================================== */

static max17048_t        fuel_gauge;
static ina226_t          power_sense;
static gpio_button_t     hw_button;
static btn_handler_t     btn_mgr;
static power_manager_t   pwr_mgr;
static bitmap_t          status_flags;
static uint8_t           flag_storage[2]; /* 16 bits */
static memory_pool_t     log_pool;
static uint8_t           pool_buf[POOL_BLOCK_SIZE * POOL_BLOCK_COUNT];
static timeout_t         meas_timer;

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. bitmap -- status flags */
    bitmap_err_t bm_err = bitmap_init(&status_flags, flag_storage, 16);
    ASSERT_EQ(bm_err, BITMAP_OK);
    ASSERT_EQ(bitmap_count_set(&status_flags), 0);

    /* 2. memory_pool -- log entry allocator */
    memory_pool_err_t mp_err = memory_pool_init(&log_pool, pool_buf,
                                                 sizeof(pool_buf),
                                                 POOL_BLOCK_SIZE);
    ASSERT_EQ(mp_err, MEMORY_POOL_OK);
    ASSERT_EQ(memory_pool_available(&log_pool), POOL_BLOCK_COUNT);

    /* 3. timeout -- measurement interval */
    mock_tick_ms = 0;
    timeout_set(&meas_timer, mock_tick_ms, MEASUREMENT_INTERVAL_MS);
    ASSERT_FALSE(timeout_expired(&meas_timer, mock_tick_ms));

    /* 4. endian -- verify conversion works (pure function) */
    uint16_t be_val = endian_htobe16(0x1234);
    uint16_t round_trip = endian_be16toh(be_val);
    ASSERT_EQ_HEX(round_trip, 0x1234);

    /* 5. max17048 -- fuel gauge */
    max_mock_reset();
    max17048_err_t fg_err = max17048_init(&fuel_gauge, &max_i2c);
    ASSERT_EQ(fg_err, MAX17048_OK);

    /* 6. ina226 -- precision current/voltage */
    ina_mock_reset();
    ina226_cfg_t ina_cfg = {
        .addr                  = INA226_ADDR_DEFAULT,
        .shunt_resistance_mohm = 50,
        .max_current_ma        = 3000,
        .averaging             = INA226_AVG_1,
        .bus_conv_time         = INA226_CT_1100US,
        .shunt_conv_time       = INA226_CT_1100US,
    };
    ina226_err_t in_err = ina226_init(&power_sense, &ina_i2c, &ina_cfg);
    ASSERT_EQ(in_err, INA226_OK);

    /* 7. gpio_button -- hardware button */
    mock_button_state = 0;
    gpio_button_cfg_t btn_cfg = gpio_button_default_cfg();
    btn_cfg.active_level = 1;
    gpio_button_init(&hw_button, &btn_hal, &btn_cfg);

    /* 8. button_handler -- multi-button event handler */
    btn_handler_err_t bh_err = btn_handler_init(&btn_mgr, btn_event_callback,
                                                 NULL,
                                                 5,    /* debounce_ticks */
                                                 100,  /* long_press_ticks */
                                                 30,   /* double_click_ticks */
                                                 50);  /* repeat_ticks */
    ASSERT_EQ(bh_err, BTN_HANDLER_OK);
    bh_err = btn_handler_register(&btn_mgr, 0, mock_btn_read, NULL);
    ASSERT_EQ(bh_err, BTN_HANDLER_OK);
    ASSERT_EQ(btn_handler_count(&btn_mgr), 1);

    /* 9. i2c_scan -- verified below in separate test */

    /* 10. power_manager -- power state machine */
    power_manager_err_t pm_err = power_manager_init(&pwr_mgr, &pm_io);
    ASSERT_EQ(pm_err, POWER_MANAGER_OK);

    /* 11. ring_log -- event log */
    ring_log_err_t rl_err = ring_log_init(&sys_log, RING_LOG_DEBUG);
    ASSERT_EQ(rl_err, RING_LOG_OK);
    ASSERT_EQ(ring_log_count(&sys_log), 0);

    printf("\n  All 11 wheels initialized successfully.\n");
}

TEST(test_i2c_scan_discover)
{
    i2c_scan_result_t result;
    i2c_scan_err_t err = i2c_scan(mock_probe, NULL, &result);
    ASSERT_EQ(err, I2C_SCAN_OK);
    ASSERT_EQ(result.count, 2);

    /* Verify discovered addresses */
    ASSERT_EQ_HEX(result.addresses[0], MAX17048_DEV_ADDR); /* 0x36 */
    ASSERT_EQ_HEX(result.addresses[1], INA226_ADDR_DEFAULT); /* 0x40 */

    /* Identify known devices */
    const char *name0 = i2c_scan_identify(result.addresses[1]);
    ASSERT_NOT_NULL(name0);
    printf("\n  I2C scan found %u devices:\n", (unsigned)result.count);
    for (uint8_t i = 0; i < result.count; i++) {
        const char *name = i2c_scan_identify(result.addresses[i]);
        printf("    0x%02X: %s\n", result.addresses[i],
               name ? name : "(unknown)");
    }

    /* Log the discovery */
    ring_log_write(&sys_log, RING_LOG_INFO, 0,
                   "I2C scan: %u devices found", (unsigned)result.count);
}

TEST(test_max17048_read_soc)
{
    /* Read full data from MAX17048 */
    max17048_data_t data;
    max17048_err_t err = max17048_read(&fuel_gauge, &data);
    ASSERT_EQ(err, MAX17048_OK);

    /* Verify SoC is ~75.5% (planted: 19328 / 256 = 75.5) */
    ASSERT_NEAR(data.soc_pct, 75.5f, 0.1f);

    /* Verify voltage is ~3.8V (planted: 48640 * 78.125e-6 ~= 3.8) */
    ASSERT_NEAR(data.voltage_v, 3.8f, 0.05f);

    /* Demonstrate endian conversion for I2C register values:
     * MAX17048 sends big-endian 16-bit; endian_be16toh converts to host */
    uint8_t raw_be[2] = { max_regs[0x02], max_regs[0x03] };
    uint16_t vcell_host = endian_read_be16(raw_be);
    float voltage_check = (float)vcell_host * 78.125e-6f;
    ASSERT_NEAR(voltage_check, data.voltage_v, 0.001f);

    /* Set status flags based on SoC */
    if (data.soc_pct < 20.0f) {
        ASSERT_EQ(bitmap_set(&status_flags, FLAG_LOW_BATTERY), BITMAP_OK);
    }
    if (data.charge_rate > 0.0f) {
        ASSERT_EQ(bitmap_set(&status_flags, FLAG_CHARGING), BITMAP_OK);
    }
    if (data.soc_pct >= 99.0f) {
        ASSERT_EQ(bitmap_set(&status_flags, FLAG_FULLY_CHARGED), BITMAP_OK);
    }

    /* SoC is 75.5%, charge_rate > 0 -> CHARGING set, LOW_BATTERY clear */
    ASSERT_TRUE(bitmap_test(&status_flags, FLAG_CHARGING));
    ASSERT_FALSE(bitmap_test(&status_flags, FLAG_LOW_BATTERY));

    ring_log_write(&sys_log, RING_LOG_INFO, 100,
                   "SoC=%.1f%% V=%.2fV rate=%.1f%%/hr",
                   (double)data.soc_pct, (double)data.voltage_v,
                   (double)data.charge_rate);

    printf("\n  MAX17048: SoC=%.1f%%, Voltage=%.3fV, Rate=%.1f%%/hr\n",
           (double)data.soc_pct, (double)data.voltage_v,
           (double)data.charge_rate);
}

TEST(test_ina226_read_power)
{
    ina226_data_t data;
    ina226_err_t err = ina226_read(&power_sense, &data);
    ASSERT_EQ(err, INA226_OK);

    /* Bus voltage: 12000 * 1.25mV = 15000mV */
    ASSERT_EQ(data.bus_voltage_mv, 15000u);

    /* Shunt voltage: 4000 * 2.5uV = 10000uV = 10mV */
    ASSERT_EQ(data.shunt_voltage_uv, 10000);

    /* Demonstrate endian conversion: INA226 registers are big-endian */
    uint8_t raw_bus[2] = { ina_regs[0x02 * 2], ina_regs[0x02 * 2 + 1] };
    uint16_t bus_raw_host = endian_read_be16(raw_bus);
    uint32_t bus_mv = (uint32_t)bus_raw_host * 125u / 100u;
    ASSERT_EQ(bus_mv, data.bus_voltage_mv);

    /* Check overcurrent threshold (e.g., > 5A) */
    if (data.current_ma > 5000) {
        ASSERT_EQ(bitmap_set(&status_flags, FLAG_OVERCURRENT), BITMAP_OK);
    }
    ASSERT_FALSE(bitmap_test(&status_flags, FLAG_OVERCURRENT));

    ring_log_write(&sys_log, RING_LOG_INFO, 200,
                   "Bus=%lumV Shunt=%lduV I=%ldmA P=%lumW",
                   (unsigned long)data.bus_voltage_mv,
                   (long)data.shunt_voltage_uv,
                   (long)data.current_ma,
                   (unsigned long)data.power_mw);

    printf("\n  INA226: Bus=%lumV, Shunt=%lduV, I=%ldmA, P=%lumW\n",
           (unsigned long)data.bus_voltage_mv,
           (long)data.shunt_voltage_uv,
           (long)data.current_ma,
           (unsigned long)data.power_mw);
}

TEST(test_bitmap_status_flags)
{
    /* CHARGING was set in test_max17048_read_soc */
    ASSERT_TRUE(bitmap_test(&status_flags, FLAG_CHARGING));
    ASSERT_EQ(bitmap_count_set(&status_flags), 1);

    /* Set multiple flags */
    ASSERT_EQ(bitmap_set(&status_flags, FLAG_ALERT_ACTIVE), BITMAP_OK);
    ASSERT_EQ(bitmap_set(&status_flags, FLAG_BUTTON_EVENT), BITMAP_OK);
    ASSERT_EQ(bitmap_count_set(&status_flags), 3);

    /* Toggle a flag */
    ASSERT_EQ(bitmap_toggle(&status_flags, FLAG_ALERT_ACTIVE), BITMAP_OK);
    ASSERT_FALSE(bitmap_test(&status_flags, FLAG_ALERT_ACTIVE));
    ASSERT_EQ(bitmap_count_set(&status_flags), 2);

    /* Clear a flag */
    ASSERT_EQ(bitmap_clear(&status_flags, FLAG_BUTTON_EVENT), BITMAP_OK);
    ASSERT_FALSE(bitmap_test(&status_flags, FLAG_BUTTON_EVENT));
    ASSERT_EQ(bitmap_count_set(&status_flags), 1);

    /* find_first_set should return FLAG_CHARGING (bit 0) */
    ASSERT_EQ(bitmap_find_first_set(&status_flags), (int)FLAG_CHARGING);

    /* Clear all and verify empty */
    bitmap_clear_all(&status_flags);
    ASSERT_EQ(bitmap_count_set(&status_flags), 0);
    ASSERT_EQ(bitmap_find_first_set(&status_flags), -1);

    printf("\n  Bitmap status flags: set/clear/toggle/find all correct.\n");
}

TEST(test_memory_pool_alloc_free)
{
    /* Pool was initialized in test_init_all with 8 blocks of 64 bytes */
    ASSERT_EQ(memory_pool_available(&log_pool), POOL_BLOCK_COUNT);
    ASSERT_EQ(memory_pool_used(&log_pool), 0u);

    /* Allocate all blocks */
    void *blocks[POOL_BLOCK_COUNT];
    for (size_t i = 0; i < POOL_BLOCK_COUNT; i++) {
        blocks[i] = memory_pool_alloc(&log_pool);
        ASSERT_NOT_NULL(blocks[i]);

        /* Write some data to verify the block is usable */
        memset(blocks[i], (int)(i + 1), POOL_BLOCK_SIZE);
    }
    ASSERT_EQ(memory_pool_available(&log_pool), 0u);
    ASSERT_EQ(memory_pool_used(&log_pool), POOL_BLOCK_COUNT);

    /* Pool is full -- next alloc returns NULL */
    void *overflow = memory_pool_alloc(&log_pool);
    ASSERT_TRUE(overflow == NULL);

    /* Free half the blocks */
    for (size_t i = 0; i < POOL_BLOCK_COUNT / 2; i++) {
        memory_pool_err_t err = memory_pool_free(&log_pool, blocks[i]);
        ASSERT_EQ(err, MEMORY_POOL_OK);
    }
    ASSERT_EQ(memory_pool_available(&log_pool), POOL_BLOCK_COUNT / 2);

    /* Re-allocate freed blocks */
    for (size_t i = 0; i < POOL_BLOCK_COUNT / 2; i++) {
        void *blk = memory_pool_alloc(&log_pool);
        ASSERT_NOT_NULL(blk);
    }
    ASSERT_EQ(memory_pool_available(&log_pool), 0u);

    /* Reset pool */
    memory_pool_reset(&log_pool);
    ASSERT_EQ(memory_pool_available(&log_pool), POOL_BLOCK_COUNT);

    printf("\n  Memory pool: alloc/free/re-alloc/reset all correct (%u blocks).\n",
           (unsigned)POOL_BLOCK_COUNT);
}

TEST(test_power_state_transition)
{
    /* Register voters: sensor and radio modules */
    uint8_t sensor_id = 0, radio_id = 0, ui_id = 0;
    power_manager_err_t err;

    err = power_manager_register(&pwr_mgr, "sensor",
                                 POWER_LEVEL_IDLE, 500, &sensor_id);
    ASSERT_EQ(err, POWER_MANAGER_OK);

    err = power_manager_register(&pwr_mgr, "radio",
                                 POWER_LEVEL_LIGHT_SLEEP, 1000, &radio_id);
    ASSERT_EQ(err, POWER_MANAGER_OK);

    err = power_manager_register(&pwr_mgr, "ui",
                                 POWER_LEVEL_IDLE, 2000, &ui_id);
    ASSERT_EQ(err, POWER_MANAGER_OK);

    /*
     * power_manager_poll() returns the deepest *allowed* sleep level:
     * - starts at SHUTDOWN (deepest)
     * - each active voter constrains it to at most voter.max_sleep
     * - the result is the shallowest active voter's max_sleep
     * - when no voters are active, result = SHUTDOWN (no constraint)
     *
     * So "all active" -> min(IDLE, LIGHT_SLEEP, IDLE) = IDLE (not ACTIVE).
     * ACTIVE is only possible if a voter explicitly sets max_sleep = ACTIVE.
     */

    /* All voters active: deepest = min(IDLE, LIGHT_SLEEP, IDLE) = IDLE */
    mock_tick_ms = 100;
    ASSERT_EQ(power_manager_activity(&pwr_mgr, sensor_id), POWER_MANAGER_OK);
    ASSERT_EQ(power_manager_activity(&pwr_mgr, radio_id), POWER_MANAGER_OK);
    ASSERT_EQ(power_manager_activity(&pwr_mgr, ui_id), POWER_MANAGER_OK);

    power_level_t level = power_manager_poll(&pwr_mgr);
    ASSERT_EQ(level, POWER_LEVEL_IDLE);
    ASSERT_EQ(power_manager_active_count(&pwr_mgr), 3);

    ring_log_write(&sys_log, RING_LOG_INFO, mock_tick_ms,
                   "Power: IDLE (%u voters)",
                   (unsigned)power_manager_active_count(&pwr_mgr));

    /* Release sensor and radio -> only UI active -> max is IDLE */
    ASSERT_EQ(power_manager_release(&pwr_mgr, sensor_id), POWER_MANAGER_OK);
    ASSERT_EQ(power_manager_release(&pwr_mgr, radio_id), POWER_MANAGER_OK);
    level = power_manager_poll(&pwr_mgr);
    ASSERT_EQ(level, POWER_LEVEL_IDLE);
    ASSERT_EQ(power_manager_active_count(&pwr_mgr), 1);

    ring_log_write(&sys_log, RING_LOG_INFO, mock_tick_ms,
                   "Power: IDLE (ui only)");

    /* Release UI -> no voters active -> no constraint -> SHUTDOWN */
    ASSERT_EQ(power_manager_release(&pwr_mgr, ui_id), POWER_MANAGER_OK);
    level = power_manager_poll(&pwr_mgr);
    ASSERT_EQ(level, POWER_LEVEL_SHUTDOWN);
    ASSERT_EQ(power_manager_active_count(&pwr_mgr), 0);

    ring_log_write(&sys_log, RING_LOG_INFO, mock_tick_ms,
                   "Power: SHUTDOWN (no voters)");

    /* Re-activate sensor -> constrained to IDLE (sensor's max_sleep) */
    ASSERT_EQ(power_manager_activity(&pwr_mgr, sensor_id), POWER_MANAGER_OK);
    level = power_manager_poll(&pwr_mgr);
    ASSERT_EQ(level, POWER_LEVEL_IDLE);

    ring_log_write(&sys_log, RING_LOG_INFO, mock_tick_ms,
                   "Power: IDLE (sensor woke)");

    /* Verify timeout auto-expiry: advance tick beyond sensor timeout (500ms) */
    mock_tick_ms = 700;
    level = power_manager_poll(&pwr_mgr);
    ASSERT_EQ(level, POWER_LEVEL_SHUTDOWN);

    printf("\n  Power state transitions: IDLE -> IDLE -> SHUTDOWN -> IDLE -> auto-expire.\n");
}

TEST(test_ring_log_events)
{
    /* Previous tests have logged several events; count should be > 0 */
    uint16_t count_before = ring_log_count(&sys_log);
    ASSERT_TRUE(count_before > 0);

    /* Write a few more structured events */
    ring_log_write(&sys_log, RING_LOG_WARN, 300, "Low battery alert: 15%%");
    ring_log_write(&sys_log, RING_LOG_ERROR, 400, "Overcurrent detected: 6.2A");
    ring_log_write(&sys_log, RING_LOG_DEBUG, 500, "Measurement tick #5");

    uint16_t count_after = ring_log_count(&sys_log);
    ASSERT_EQ(count_after, count_before + 3);

    /* Read back newest entry (index 0 = newest) */
    ring_log_entry_t entry;
    ring_log_err_t err = ring_log_read(&sys_log, 0, &entry);
    ASSERT_EQ(err, RING_LOG_OK);
    ASSERT_EQ(entry.level, RING_LOG_DEBUG);
    ASSERT_EQ(entry.timestamp, 500u);

    /* Read back oldest entry */
    err = ring_log_read_oldest(&sys_log, 0, &entry);
    ASSERT_EQ(err, RING_LOG_OK);
    ASSERT_EQ(entry.level, RING_LOG_INFO);

    /* Change min level to WARN and verify filtering */
    ring_log_set_level(&sys_log, RING_LOG_WARN);
    uint32_t dropped_before = ring_log_dropped(&sys_log);
    ring_log_write(&sys_log, RING_LOG_DEBUG, 600, "Should be filtered");
    ASSERT_EQ(ring_log_dropped(&sys_log), dropped_before + 1);
    ASSERT_EQ(ring_log_count(&sys_log), count_after); /* unchanged */

    /* WARN and above still get through */
    ring_log_write(&sys_log, RING_LOG_WARN, 700, "This passes filter");
    ASSERT_EQ(ring_log_count(&sys_log), count_after + 1);

    /* Verify total lifetime writes */
    ASSERT_TRUE(ring_log_total(&sys_log) > 0);

    printf("\n  Ring log: %u entries stored, %lu total writes, %lu dropped.\n",
           ring_log_count(&sys_log),
           (unsigned long)ring_log_total(&sys_log),
           (unsigned long)ring_log_dropped(&sys_log));

    /* Dump last few entries */
    printf("  --- Recent log entries (newest first) ---\n");
    uint16_t to_show = ring_log_count(&sys_log);
    if (to_show > 5) to_show = 5;
    for (uint16_t i = 0; i < to_show; i++) {
        ring_log_entry_t e;
        if (ring_log_read(&sys_log, i, &e) == RING_LOG_OK) {
            const char *lvl[] = { "DBG", "INF", "WRN", "ERR" };
            printf("    [%u] %s @%u: %s\n", (unsigned)i,
                   lvl[e.level], (unsigned)e.timestamp, e.message);
        }
    }
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("power_monitor integration")
    RUN(test_init_all);
    RUN(test_i2c_scan_discover);
    RUN(test_max17048_read_soc);
    RUN(test_ina226_read_power);
    RUN(test_bitmap_status_flags);
    RUN(test_memory_pool_alloc_free);
    RUN(test_power_state_transition);
    RUN(test_ring_log_events);
TEST_MAIN_END()
