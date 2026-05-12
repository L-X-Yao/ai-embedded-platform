// SPDX-License-Identifier: MIT
/*
 * industrial_monitor -- integration recipe: CAN bus monitoring with PID control
 *
 * Demonstrates eight wheels working together:
 *   L0  pid_controller  -- closed-loop PID control
 *   L0  rate_limiter    -- token-bucket CAN message throttling
 *   L0  fixed_point     -- Q16.16 math for ADC scaling
 *   L1  ina219          -- bus voltage + current measurement
 *   L1  ads1115         -- 16-bit ADC temperature reading
 *   L2  canbus          -- CAN 2.0A frame encode/decode
 *   L3  telemetry       -- structured telemetry collection
 *   L3  task_watchdog   -- control-loop deadline monitoring
 *
 * Runs entirely on host with mock I2C + loopback CAN -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/industrial_monitor
 */

#include "test_harness.h"

/* wheels */
#include "pid_controller.h"
#include "rate_limiter.h"
#include "fixed_point.h"
#include "ina219.h"
#include "ads1115.h"
#include "canbus.h"
#include "telemetry.h"
#include "task_watchdog.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define CONTROL_LOOP_CYCLES   10   /* number of PID control iterations       */
#define CAN_LOOPBACK_SIZE    128   /* loopback buffer for CAN frames (bytes) */
#define WD_TIMEOUT_TICKS      50   /* watchdog timeout for control loop      */

/* CAN message IDs for the industrial bus */
#define CAN_ID_VOLTAGE   0x100u
#define CAN_ID_CURRENT   0x101u
#define CAN_ID_TEMP      0x102u
#define CAN_ID_PID_OUT   0x103u

/* ======================================================================
 * Mock I2C -- shared register-based backend for INA219 + ADS1115
 *
 * INA219 uses write_regs/read_regs (multi-byte, big-endian 16-bit regs).
 * ADS1115 uses write_regs/read_regs + write_reg (single byte, but the
 * driver also calls write_regs for 16-bit config writes).
 * ====================================================================== */

/* INA219 mock registers (16-bit, stored as pairs at reg*2) */
static uint8_t ina_regs[16];  /* 6 registers x 2 bytes + padding */
static int     ina_op_count;

static void ina_mock_reset(void)
{
    memset(ina_regs, 0, sizeof(ina_regs));
    ina_op_count = 0;

    /* Plant bus voltage = 12.0V in register 0x02.
     * Bus voltage register: value = raw >> 3, LSB = 4 mV.
     * 12.0V / 0.004 = 3000 -> raw = 3000 << 3 = 24000 = 0x5DC0 */
    ina_regs[0x02 * 2 + 0] = 0x5D;
    ina_regs[0x02 * 2 + 1] = 0xC0;

    /* Plant shunt voltage = 5.0 mV in register 0x01.
     * Shunt register: signed 16-bit, LSB = 10 uV = 0.01 mV.
     * 5.0 mV / 0.01 = 500 = 0x01F4 */
    ina_regs[0x01 * 2 + 0] = 0x01;
    ina_regs[0x01 * 2 + 1] = 0xF4;

    /* Plant current = 50 mA in register 0x04.
     * Current register: signed 16-bit, LSB = current_lsb (set during init).
     * With shunt=0.1 ohm, gain=320mV: max_current=3.2A, current_lsb=3.2/32768.
     * raw = 50e-3 / (3.2/32768) / 1000 ... simpler: just plant a known raw.
     * We plant raw=512 -> current = 512 * current_lsb * 1000 mA.
     * With current_lsb = 3.2/32768 ~ 9.77e-5 A, current ~ 50.0 mA */
    ina_regs[0x04 * 2 + 0] = 0x02;
    ina_regs[0x04 * 2 + 1] = 0x00;
}

static bool ina_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ina_op_count++;
    for (uint16_t i = 0; i < len && (reg * 2u + i) < sizeof(ina_regs); i++) {
        ina_regs[reg * 2u + i] = buf[i];
    }
    return true;
}

static bool ina_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ina_op_count++;
    for (uint16_t i = 0; i < len && (reg * 2u + i) < sizeof(ina_regs); i++) {
        buf[i] = ina_regs[reg * 2u + i];
    }
    return true;
}

static const ina219_i2c_t ina_i2c = {
    .read_regs  = ina_read_regs,
    .write_regs = ina_write_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

/* ADS1115 mock registers */
static uint8_t ads_regs[8];  /* reg 0x00 (conversion) + 0x01 (config) x 2 bytes */
static int     ads_op_count;

static void ads_mock_reset(void)
{
    memset(ads_regs, 0, sizeof(ads_regs));
    ads_op_count = 0;

    /* Plant ADC raw = 10000 in conversion register (0x00).
     * With PGA_4096: voltage_mv = 10000 * (4096/32768) = 1250.0 mV.
     * Simulating a temperature sensor: 1250 mV ~ 125.0 C at 10 mV/C. */
    ads_regs[0] = 0x27;  /* 10000 >> 8 = 0x27 */
    ads_regs[1] = 0x10;  /* 10000 & 0xFF = 0x10 */

    /* Config register at 0x01: bit 15 = 1 (conversion ready) */
    ads_regs[2] = 0x85;
    ads_regs[3] = 0x83;
}

static bool ads_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    ads_op_count++;
    if (reg * 2u < sizeof(ads_regs)) {
        ads_regs[reg * 2u] = val;
    }
    return true;
}

static bool ads_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ads_op_count++;
    for (uint16_t i = 0; i < len && (reg * 2u + i) < sizeof(ads_regs); i++) {
        buf[i] = ads_regs[reg * 2u + i];
    }
    return true;
}

static bool ads_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ads_op_count++;
    for (uint16_t i = 0; i < len && (reg * 2u + i) < sizeof(ads_regs); i++) {
        ads_regs[reg * 2u + i] = buf[i];
    }
    return true;
}

static const ads1115_i2c_t ads_i2c = {
    .write_reg  = ads_write_reg,
    .read_regs  = ads_read_regs,
    .write_regs = ads_write_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

/* ======================================================================
 * Mock CAN -- loopback buffer
 *
 * canbus_frame_pack() writes into a static loopback buffer.
 * canbus_frame_unpack() reads it back for verification.
 * ====================================================================== */

static uint8_t  can_loopback[CAN_LOOPBACK_SIZE];
static uint16_t can_loopback_len;
static int      can_frame_count;

static void can_loopback_reset(void)
{
    memset(can_loopback, 0, sizeof(can_loopback));
    can_loopback_len = 0;
    can_frame_count = 0;
}

/* ======================================================================
 * Telemetry send callback -- captures output for verification
 * ====================================================================== */

static char     telem_output[512];
static uint16_t telem_output_len;

static telemetry_err_t telem_send(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    if (len >= sizeof(telem_output)) {
        return TELEMETRY_ERR_OVERFLOW;
    }
    memcpy(telem_output, data, len);
    telem_output[len] = '\0';
    telem_output_len = len;
    return TELEMETRY_OK;
}

/* ======================================================================
 * Watchdog expiry callback -- records expired task name
 * ====================================================================== */

static const char *wd_expired_name;
static int         wd_expired_count;

static void wd_on_expired(void *ctx, const char *task_name, uint32_t last_feed)
{
    (void)ctx; (void)last_feed;
    wd_expired_name = task_name;
    wd_expired_count++;
}

/* ======================================================================
 * Global wheel instances
 * ====================================================================== */

static pid_controller_t pid;
static rate_limiter_t   limiter;
static ina219_t         ina_dev;
static ads1115_t        ads_dev;
static telemetry_t      telem;
static task_watchdog_t  wd;

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. PID controller: Kp=2.0, Ki=0.5, Kd=0.1, output 0-100 */
    pid_controller_cfg_t pid_cfg = {
        .kp           = 2.0f,
        .ki           = 0.5f,
        .kd           = 0.1f,
        .output_min   = 0.0f,
        .output_max   = 100.0f,
        .integral_min = -50.0f,
        .integral_max =  50.0f,
    };
    pid_controller_err_t pid_err = pid_controller_init(&pid, &pid_cfg);
    ASSERT_EQ(pid_err, PID_CONTROLLER_OK);

    /* 2. Rate limiter: 5 tokens max, 1 token/tick */
    rate_limiter_err_t rl_err = rate_limiter_init(&limiter, 5, 1);
    ASSERT_EQ(rl_err, RATE_LIMITER_OK);

    /* 3. INA219: addr 0x40, shunt 0.1 ohm, 32V range, 320mV gain */
    ina_mock_reset();
    ina219_err_t ina_err = ina219_init(&ina_dev, &ina_i2c, 0x40, 0.1f,
                                       INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina_err, INA219_OK);

    /* 4. ADS1115: addr 0x48, AIN0-GND, PGA 4.096V, 128 SPS */
    ads_mock_reset();
    ads1115_cfg_t ads_cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_4096,
        .rate = ADS1115_RATE_128,
    };
    ads1115_err_t ads_err = ads1115_init(&ads_dev, &ads_i2c, &ads_cfg);
    ASSERT_EQ(ads_err, ADS1115_OK);

    /* 5. fixed_point: no init needed, verify basic conversion */
    fp_t val = fp_from_float(12.5f);
    ASSERT_NEAR(fp_to_float(val), 12.5f, 0.001f);

    /* 6. canbus: stateless, no init -- verify ID helper */
    ASSERT_TRUE(canbus_is_standard_id(CAN_ID_VOLTAGE));

    /* 7. Telemetry: JSON format with send callback */
    telemetry_err_t tel_err = telemetry_init(&telem, telem_send, NULL,
                                             TELEMETRY_FMT_JSON);
    ASSERT_EQ(tel_err, TELEMETRY_OK);

    tel_err = telemetry_register(&telem, "bus_v");
    ASSERT_EQ(tel_err, TELEMETRY_OK);
    tel_err = telemetry_register(&telem, "current_ma");
    ASSERT_EQ(tel_err, TELEMETRY_OK);
    tel_err = telemetry_register(&telem, "temp_c");
    ASSERT_EQ(tel_err, TELEMETRY_OK);
    tel_err = telemetry_register(&telem, "pid_out");
    ASSERT_EQ(tel_err, TELEMETRY_OK);

    /* 8. Task watchdog */
    task_wd_err_t wd_err = task_watchdog_init(&wd);
    ASSERT_EQ(wd_err, TASK_WD_OK);

    wd_expired_name = NULL;
    wd_expired_count = 0;
    task_watchdog_set_callback(&wd, wd_on_expired, NULL);

    wd_err = task_watchdog_register(&wd, "control_loop", WD_TIMEOUT_TICKS);
    ASSERT_EQ(wd_err, TASK_WD_OK);
    ASSERT_EQ(task_watchdog_count(&wd), 1);
}

TEST(test_ina219_read)
{
    /* Re-plant known values after init overwrote config/calibration regs */
    ina_regs[0x02 * 2 + 0] = 0x5D;
    ina_regs[0x02 * 2 + 1] = 0xC0;
    ina_regs[0x01 * 2 + 0] = 0x01;
    ina_regs[0x01 * 2 + 1] = 0xF4;

    /* Read bus voltage -- expect ~12.0V */
    float voltage_v = 0.0f;
    ina219_err_t err = ina219_read_bus_voltage(&ina_dev, &voltage_v);
    ASSERT_EQ(err, INA219_OK);
    ASSERT_NEAR(voltage_v, 12.0f, 0.1f);

    /* Read shunt voltage -- expect ~5.0 mV */
    float shunt_mv = 0.0f;
    err = ina219_read_shunt_voltage(&ina_dev, &shunt_mv);
    ASSERT_EQ(err, INA219_OK);
    ASSERT_NEAR(shunt_mv, 5.0f, 0.1f);

    printf("\n  INA219: bus=%.2fV  shunt=%.2fmV\n",
           (double)voltage_v, (double)shunt_mv);
}

TEST(test_ads1115_read)
{
    /* Re-plant conversion register and config-ready flag */
    ads_regs[0] = 0x27;  /* raw 10000 high byte */
    ads_regs[1] = 0x10;  /* raw 10000 low byte */
    ads_regs[2] = 0x85;  /* config with bit 15 set (ready) */
    ads_regs[3] = 0x83;

    /* Read raw ADC value */
    int16_t raw = 0;
    ads1115_err_t err = ads1115_read_raw(&ads_dev, &raw);
    ASSERT_EQ(err, ADS1115_OK);
    ASSERT_EQ(raw, 10000);

    /* Convert to voltage: PGA_4096 scale = 4096/32768 = 0.125 mV/LSB.
     * 10000 * 0.125 = 1250.0 mV */
    float voltage_mv = 0.0f;

    /* Re-plant for voltage read (read_raw consumed it) */
    ads_regs[0] = 0x27;
    ads_regs[1] = 0x10;
    ads_regs[2] = 0x85;
    ads_regs[3] = 0x83;

    err = ads1115_read_voltage(&ads_dev, &voltage_mv);
    ASSERT_EQ(err, ADS1115_OK);
    ASSERT_NEAR(voltage_mv, 1250.0f, 1.0f);

    /* Use fixed_point to scale mV to temperature (10 mV/C sensor) */
    fp_t fp_mv = fp_from_float(voltage_mv);
    fp_t fp_scale = fp_from_float(0.1f);   /* 1/10 mV per degree */
    fp_t fp_temp = fp_mul(fp_mv, fp_scale);
    float temp_c = fp_to_float(fp_temp);
    ASSERT_NEAR(temp_c, 125.0f, 1.0f);

    printf("  ADS1115: raw=%d  voltage=%.1fmV  temp(fp)=%.1fC\n",
           (int)raw, (double)voltage_mv, (double)temp_c);
}

TEST(test_pid_control_loop)
{
    /* Reset PID state for clean test */
    pid_controller_err_t err = pid_controller_reset(&pid);
    ASSERT_EQ(err, PID_CONTROLLER_OK);

    /* Setpoint: 25.0 C, starting measurement: 20.0 C, dt = 0.1s */
    float setpoint = 25.0f;
    float measured = 20.0f;
    float dt = 0.1f;
    float output;

    printf("\n  --- PID Control Loop ---\n");
    printf("  step  setpoint  measured   error  output\n");
    printf("  ----  --------  --------  ------  ------\n");

    for (int i = 0; i < CONTROL_LOOP_CYCLES; i++) {
        output = pid_controller_update(&pid, setpoint, measured, dt);

        printf("  %4d    %5.1f    %6.2f    %5.2f  %6.2f\n",
               i, (double)setpoint, (double)measured,
               (double)(setpoint - measured), (double)output);

        /* Output is clamped to [0, 100] */
        ASSERT_TRUE(output >= 0.0f && output <= 100.0f);

        /* Simulate plant: measurement moves toward setpoint proportional
         * to control output (simple first-order model) */
        measured += output * 0.02f;
    }

    /* After several iterations, measured should move toward setpoint.
     * With Kp=2, output~10, step=0.02 per iter, expect ~2 units of movement. */
    ASSERT_TRUE(measured > 21.0f);

    /* First iteration with error=5.0: P=2*5=10, I=0 (first-run skip),
     * D=0 (first run). Output should be positive and meaningful. */
}

TEST(test_canbus_frame_encode)
{
    can_loopback_reset();

    /* Build a standard CAN frame carrying voltage data (12.0V as uint16 mV) */
    canbus_frame_t tx_frame;
    memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.id       = CAN_ID_VOLTAGE;
    tx_frame.extended = false;
    tx_frame.rtr      = false;
    tx_frame.dlc      = 4;

    /* Pack voltage (12000 mV) and current (50 mA) as 16-bit LE signals */
    memset(tx_frame.data, 0, sizeof(tx_frame.data));
    canbus_signal_pack(tx_frame.data, 0, 16, false, 12000);  /* voltage_mv */
    canbus_signal_pack(tx_frame.data, 16, 16, false, 50);    /* current_ma */

    /* Validate frame */
    canbus_err_t can_err = canbus_frame_validate(&tx_frame);
    ASSERT_EQ(can_err, CANBUS_OK);

    /* Pack into wire format */
    int packed_len = canbus_frame_pack(&tx_frame, can_loopback,
                                       CAN_LOOPBACK_SIZE);
    ASSERT_TRUE(packed_len > 0);
    can_loopback_len = (uint16_t)packed_len;
    can_frame_count++;

    /* Unpack and verify round-trip */
    canbus_frame_t rx_frame;
    memset(&rx_frame, 0, sizeof(rx_frame));
    can_err = canbus_frame_unpack(can_loopback, can_loopback_len, &rx_frame);
    ASSERT_EQ(can_err, CANBUS_OK);
    ASSERT_EQ(rx_frame.id, CAN_ID_VOLTAGE);
    ASSERT_EQ(rx_frame.dlc, 4);
    ASSERT_FALSE(rx_frame.extended);

    /* Extract signals back */
    uint64_t rx_voltage = canbus_signal_extract(rx_frame.data, 0, 16, false);
    uint64_t rx_current = canbus_signal_extract(rx_frame.data, 16, 16, false);
    ASSERT_EQ(rx_voltage, 12000);
    ASSERT_EQ(rx_current, 50);

    printf("\n  CAN frame: ID=0x%03X DLC=%d  voltage=%u mV  current=%u mA\n",
           (unsigned)rx_frame.id, (int)rx_frame.dlc,
           (unsigned)rx_voltage, (unsigned)rx_current);
}

TEST(test_rate_limiter_throttle)
{
    /* Reset limiter for clean test */
    rate_limiter_reset(&limiter);

    /* Bucket: 5 tokens max, 1 token/tick. Start full. */
    uint32_t tick = 0;

    /* Consume all 5 tokens rapidly at tick=0 */
    for (int i = 0; i < 5; i++) {
        rate_limiter_err_t err = rate_limiter_try(&limiter, tick);
        ASSERT_EQ(err, RATE_LIMITER_OK);
    }
    ASSERT_EQ(rate_limiter_allowed(&limiter), 5);

    /* 6th attempt at same tick should be denied */
    rate_limiter_err_t err = rate_limiter_try(&limiter, tick);
    ASSERT_EQ(err, RATE_LIMITER_DENIED);
    ASSERT_EQ(rate_limiter_denied(&limiter), 1);

    /* Advance 3 ticks -- refills 3 tokens */
    tick += 3;
    err = rate_limiter_try(&limiter, tick);
    ASSERT_EQ(err, RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_allowed(&limiter), 6);

    /* Verify available count after consuming 1 of 3 refilled tokens */
    ASSERT_EQ(rate_limiter_available(&limiter), 2);

    printf("\n  Rate limiter: allowed=%u  denied=%u  available=%u\n",
           (unsigned)rate_limiter_allowed(&limiter),
           (unsigned)rate_limiter_denied(&limiter),
           (unsigned)rate_limiter_available(&limiter));
}

TEST(test_telemetry_collect)
{
    /* Update telemetry fields */
    telemetry_err_t err;
    err = telemetry_update(&telem, "bus_v", 12.0f);
    ASSERT_EQ(err, TELEMETRY_OK);
    err = telemetry_update(&telem, "current_ma", 50.0f);
    ASSERT_EQ(err, TELEMETRY_OK);
    err = telemetry_update(&telem, "temp_c", 125.0f);
    ASSERT_EQ(err, TELEMETRY_OK);
    err = telemetry_update(&telem, "pid_out", 42.5f);
    ASSERT_EQ(err, TELEMETRY_OK);

    /* Publish -- calls telem_send callback */
    memset(telem_output, 0, sizeof(telem_output));
    err = telemetry_publish(&telem, 1000);
    ASSERT_EQ(err, TELEMETRY_OK);
    ASSERT_EQ(telemetry_get_send_count(&telem), 1);

    /* Verify JSON output contains expected fields */
    ASSERT_TRUE(telem_output_len > 0);
    ASSERT_TRUE(strstr(telem_output, "\"ts\":1000") != NULL);
    ASSERT_TRUE(strstr(telem_output, "\"bus_v\":12") != NULL);
    ASSERT_TRUE(strstr(telem_output, "\"current_ma\":50") != NULL);
    ASSERT_TRUE(strstr(telem_output, "\"temp_c\":125") != NULL);
    ASSERT_TRUE(strstr(telem_output, "\"pid_out\":42.5") != NULL);

    printf("\n  Telemetry JSON: %s\n", telem_output);
}

TEST(test_watchdog_feed_and_check)
{
    /* Advance a few ticks and feed -- should stay healthy */
    for (int i = 0; i < 10; i++) {
        task_watchdog_tick(&wd);
    }
    task_wd_err_t wd_err = task_watchdog_feed(&wd, "control_loop");
    ASSERT_EQ(wd_err, TASK_WD_OK);

    /* Check -- should not expire (only 10 ticks since last feed < 50 timeout) */
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "control_loop"), TASK_WD_STATUS_OK);
    ASSERT_EQ(task_watchdog_expired_count(&wd), 0);
    ASSERT_EQ(wd_expired_count, 0);

    /* Now advance past timeout WITHOUT feeding */
    for (int i = 0; i < WD_TIMEOUT_TICKS + 1; i++) {
        task_watchdog_tick(&wd);
    }
    task_watchdog_check(&wd);

    /* Should have expired and fired callback */
    ASSERT_EQ(task_watchdog_status(&wd, "control_loop"), TASK_WD_STATUS_EXPIRED);
    ASSERT_EQ(task_watchdog_expired_count(&wd), 1);
    ASSERT_EQ(wd_expired_count, 1);
    ASSERT_NOT_NULL(wd_expired_name);
    ASSERT_STR_EQ(wd_expired_name, "control_loop");

    /* Feed again to recover */
    wd_err = task_watchdog_feed(&wd, "control_loop");
    ASSERT_EQ(wd_err, TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "control_loop"), TASK_WD_STATUS_OK);

    printf("\n  Watchdog: expired=%u, recovered after feed\n",
           (unsigned)wd_expired_count);
}

TEST(test_full_integration_cycle)
{
    /* Run a complete cycle: read sensors -> PID -> rate-limit -> CAN -> telemetry
     * This ties all 8 wheels together in a realistic control loop pattern. */

    pid_controller_reset(&pid);
    rate_limiter_reset(&limiter);
    can_loopback_reset();
    telemetry_reset(&telem);

    /* Re-plant INA219 mock values */
    ina_regs[0x02 * 2 + 0] = 0x5D;
    ina_regs[0x02 * 2 + 1] = 0xC0;

    /* Re-plant ADS1115 mock values */
    ads_regs[0] = 0x27;
    ads_regs[1] = 0x10;
    ads_regs[2] = 0x85;
    ads_regs[3] = 0x83;

    printf("\n  --- Full Integration Cycle ---\n");

    float setpoint = 25.0f;
    float measured = 20.0f;
    uint32_t tick = 0;

    for (int cycle = 0; cycle < 5; cycle++) {
        /* Step 1: Read INA219 bus voltage */
        float bus_v = 0.0f;
        ina219_err_t ina_err = ina219_read_bus_voltage(&ina_dev, &bus_v);
        ASSERT_EQ(ina_err, INA219_OK);

        /* Step 2: Read ADS1115 and convert with fixed_point */
        ads_regs[0] = 0x27;
        ads_regs[1] = 0x10;
        ads_regs[2] = 0x85;
        ads_regs[3] = 0x83;
        float adc_mv = 0.0f;
        ads1115_err_t ads_err = ads1115_read_voltage(&ads_dev, &adc_mv);
        ASSERT_EQ(ads_err, ADS1115_OK);

        fp_t fp_mv = fp_from_float(adc_mv);
        fp_t fp_temp = fp_mul(fp_mv, fp_from_float(0.1f));
        float temp_c = fp_to_float(fp_temp);

        /* Step 3: PID update */
        float pid_out = pid_controller_update(&pid, setpoint, measured, 0.1f);
        measured += pid_out * 0.02f;

        /* Step 4: Rate-limit CAN transmission */
        tick += 2;
        rate_limiter_err_t rl_err = rate_limiter_try(&limiter, tick);
        if (rl_err == RATE_LIMITER_OK) {
            /* Step 5: Build and pack CAN frame */
            canbus_frame_t frame;
            memset(&frame, 0, sizeof(frame));
            frame.id       = CAN_ID_VOLTAGE;
            frame.extended = false;
            frame.rtr      = false;
            frame.dlc      = 8;
            memset(frame.data, 0, 8);
            canbus_signal_pack(frame.data, 0, 16, false,
                               (uint64_t)(bus_v * 1000.0f));
            canbus_signal_pack(frame.data, 16, 16, false,
                               (uint64_t)(temp_c * 10.0f));
            canbus_signal_pack(frame.data, 32, 16, false,
                               (uint64_t)(pid_out * 10.0f));

            int len = canbus_frame_pack(&frame, can_loopback,
                                        CAN_LOOPBACK_SIZE);
            ASSERT_TRUE(len > 0);
            can_frame_count++;
        }

        /* Step 6: Update telemetry */
        ASSERT_EQ(telemetry_update(&telem, "bus_v", bus_v), TELEMETRY_OK);
        ASSERT_EQ(telemetry_update(&telem, "current_ma", 50.0f), TELEMETRY_OK);
        ASSERT_EQ(telemetry_update(&telem, "temp_c", temp_c), TELEMETRY_OK);
        ASSERT_EQ(telemetry_update(&telem, "pid_out", pid_out), TELEMETRY_OK);

        /* Step 7: Feed watchdog */
        task_watchdog_tick(&wd);
        ASSERT_EQ(task_watchdog_feed(&wd, "control_loop"), TASK_WD_OK);

        printf("  cycle %d: V=%.1f  T=%.1fC  PID=%.1f  measured=%.2f\n",
               cycle, (double)bus_v, (double)temp_c,
               (double)pid_out, (double)measured);
    }

    /* Publish final telemetry */
    telemetry_err_t tel_err = telemetry_publish(&telem, tick);
    ASSERT_EQ(tel_err, TELEMETRY_OK);
    ASSERT_TRUE(telemetry_get_send_count(&telem) > 0);

    /* Watchdog should still be healthy */
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "control_loop"), TASK_WD_STATUS_OK);

    printf("  Final telemetry: %s\n", telem_output);
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("industrial_monitor integration")
    RUN(test_init_all);
    RUN(test_ina219_read);
    RUN(test_ads1115_read);
    RUN(test_pid_control_loop);
    RUN(test_canbus_frame_encode);
    RUN(test_rate_limiter_throttle);
    RUN(test_telemetry_collect);
    RUN(test_watchdog_feed_and_check);
    RUN(test_full_integration_cycle);
TEST_MAIN_END()
