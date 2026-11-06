// SPDX-License-Identifier: MIT
/*
 * motion_control_adv -- integration recipe: advanced multi-actuator motion
 *                       control with IMU feedback
 *
 * Demonstrates six L1 wheels working together:
 *   L1  mpu6050     -- 6-axis IMU (accel + gyro)
 *   L1  servo       -- PWM servo control
 *   L1  stepper     -- Stepper motor with trapezoidal acceleration
 *   L1  pca9685     -- 16-channel I2C PWM driver (drives servos)
 *   L1  drv8833     -- Dual H-bridge DC motor driver
 *   L1  drv2605     -- Haptic motor driver (vibration feedback)
 *
 * Integration flow:
 *   1. Init all actuators and the IMU
 *   2. Read IMU accel/gyro data
 *   3. Compute tilt angle from accelerometer
 *   4. PCA9685 drives servo positions based on tilt
 *   5. Stepper moves to a target position
 *   6. DRV8833 controls DC motors with direction
 *   7. DRV2605 provides haptic feedback on motion completion
 *
 * Runs entirely on host with mocked hardware -- no real MCU required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/motion_control_adv
 */

#include "test_harness.h"

/* wheels */
#include "mpu6050.h"
#include "servo.h"
#include "stepper.h"
#include "pca9685.h"
#include "drv8833.h"
#include "drv2605.h"

#include <string.h>
#include <math.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define SERVO_MIN_PULSE_US   500u
#define SERVO_MAX_PULSE_US  2500u
#define SERVO_MAX_ANGLE      180u

#define STEPPER_MAX_SPEED    800u   /* steps/sec */
#define STEPPER_ACCEL        400u   /* steps/sec^2 */

#define DRV8833_MAX_DUTY    1000u   /* 0.1% resolution */

#define PCA9685_FREQ          50u   /* 50 Hz for servos */

/* ======================================================================
 * Mock I2C -- MPU6050 (register-based: write_reg / read_regs)
 * ====================================================================== */

static uint8_t mpu_regs[256];
static int     mpu_op_count;

static void mpu_mock_reset(void)
{
    memset(mpu_regs, 0, sizeof(mpu_regs));
    mpu_op_count = 0;

    /* WHO_AM_I register (0x75) = 0x68 */
    mpu_regs[0x75] = 0x68;
}

/* Plant raw accel data into registers 0x3B..0x40 (big-endian int16) */
static void mpu_plant_accel(int16_t ax, int16_t ay, int16_t az)
{
    mpu_regs[0x3B] = (uint8_t)((uint16_t)ax >> 8);
    mpu_regs[0x3C] = (uint8_t)((uint16_t)ax & 0xFF);
    mpu_regs[0x3D] = (uint8_t)((uint16_t)ay >> 8);
    mpu_regs[0x3E] = (uint8_t)((uint16_t)ay & 0xFF);
    mpu_regs[0x3F] = (uint8_t)((uint16_t)az >> 8);
    mpu_regs[0x40] = (uint8_t)((uint16_t)az & 0xFF);
}

/* Plant raw gyro data into registers 0x43..0x48 (big-endian int16) */
static void mpu_plant_gyro(int16_t gx, int16_t gy, int16_t gz)
{
    mpu_regs[0x43] = (uint8_t)((uint16_t)gx >> 8);
    mpu_regs[0x44] = (uint8_t)((uint16_t)gx & 0xFF);
    mpu_regs[0x45] = (uint8_t)((uint16_t)gy >> 8);
    mpu_regs[0x46] = (uint8_t)((uint16_t)gy & 0xFF);
    mpu_regs[0x47] = (uint8_t)((uint16_t)gz >> 8);
    mpu_regs[0x48] = (uint8_t)((uint16_t)gz & 0xFF);
}

static bool mpu_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    mpu_op_count++;
    mpu_regs[reg] = val;
    return true;
}

static bool mpu_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    mpu_op_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = mpu_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const mpu6050_i2c_t mpu_i2c = {
    .write_reg = mpu_write_reg,
    .read_regs = mpu_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock I2C -- PCA9685 (register-based)
 * ====================================================================== */

static uint8_t pca_regs[256];
static int     pca_op_count;

static void pca_mock_reset(void)
{
    memset(pca_regs, 0, sizeof(pca_regs));
    pca_op_count = 0;

    /* MODE1 default after power-on: SLEEP=1, ALLCALL=1 */
    pca_regs[0x00] = 0x11;
}

static bool pca_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    pca_op_count++;
    pca_regs[reg] = val;
    return true;
}

static bool pca_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    pca_op_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = pca_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static void pca_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static const pca9685_i2c_t pca_i2c = {
    .write_reg = pca_write_reg,
    .read_regs = pca_read_regs,
    .delay_ms  = pca_delay_ms,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock I2C -- DRV2605 (register-based)
 * ====================================================================== */

static uint8_t haptic_regs[256];
static int     haptic_op_count;

static void haptic_mock_reset(void)
{
    memset(haptic_regs, 0, sizeof(haptic_regs));
    haptic_op_count = 0;

    /* Status register (0x00): device ID bits [7:5] = 7 (DRV2605L) */
    haptic_regs[0x00] = (uint8_t)(7u << 5);
}

static bool haptic_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                              uint8_t val)
{
    (void)ctx; (void)dev_addr;
    haptic_op_count++;
    haptic_regs[reg] = val;
    return true;
}

static bool haptic_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                              uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    haptic_op_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = haptic_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const drv2605_i2c_t haptic_i2c = {
    .write_reg = haptic_write_reg,
    .read_regs = haptic_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock PWM -- Servo (set_pulse_us callback)
 * ====================================================================== */

static uint32_t servo_last_pulse_us;
static int      servo_pwm_calls;

static bool servo_mock_set_pulse(void *ctx, uint32_t pulse_us)
{
    (void)ctx;
    servo_last_pulse_us = pulse_us;
    servo_pwm_calls++;
    return true;
}

static const servo_pwm_t servo_pwm = {
    .set_pulse_us = servo_mock_set_pulse,
    .ctx          = NULL,
};

/* ======================================================================
 * Mock GPIO/PWM -- Stepper (step_pin / dir_pin / delay_us)
 * ====================================================================== */

static int  stepper_step_count;
static bool stepper_last_dir;
static bool stepper_last_step_level;

static void stepper_mock_step_pin(void *ctx, bool level)
{
    (void)ctx;
    stepper_last_step_level = level;
    if (level) {
        stepper_step_count++;
    }
}

static void stepper_mock_dir_pin(void *ctx, bool level)
{
    (void)ctx;
    stepper_last_dir = level;
}

static void stepper_mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx; (void)us;
    /* no-op in mock: don't actually wait */
}

static const stepper_hal_t stepper_hal = {
    .step_pin = stepper_mock_step_pin,
    .dir_pin  = stepper_mock_dir_pin,
    .delay_us = stepper_mock_delay_us,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock PWM -- DRV8833 (set_pwm callback)
 * ====================================================================== */

/* Track last PWM duty per channel and pin: [channel][pin] */
static uint16_t drv_pwm_state[2][2];
static int      drv_pwm_calls;
static bool     drv_sleep_state;

static void drv_mock_set_pwm(void *ctx, uint8_t channel, uint8_t pin,
                              uint16_t duty)
{
    (void)ctx;
    if (channel < 2 && pin < 2) {
        drv_pwm_state[channel][pin] = duty;
    }
    drv_pwm_calls++;
}

static void drv_mock_set_sleep(void *ctx, bool sleep)
{
    (void)ctx;
    drv_sleep_state = sleep;
}

static void drv_mock_reset(void)
{
    memset(drv_pwm_state, 0, sizeof(drv_pwm_state));
    drv_pwm_calls = 0;
    drv_sleep_state = false;
}

static const drv8833_hal_t drv_hal = {
    .set_pwm   = drv_mock_set_pwm,
    .set_sleep = drv_mock_set_sleep,
    .ctx       = NULL,
};

/* ======================================================================
 * Global instances
 * ====================================================================== */

static mpu6050_t  mpu_dev;
static servo_t    servo_dev;
static stepper_t  stepper_dev;
static pca9685_t  pca_dev;
static drv8833_t  drv_dev;
static drv2605_t  haptic_dev;

/* ======================================================================
 * Helper: compute tilt angle from accelerometer
 * ====================================================================== */

static float compute_tilt_deg(float ax_g, float az_g)
{
    /* atan2(ax, az) gives pitch angle in radians, convert to degrees */
    return (float)(atan2((double)ax_g, (double)az_g) * 180.0 / 3.14159265358979);
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. MPU6050 */
    mpu_mock_reset();
    /* Plant plausible accel: ~0G X, ~0G Y, ~1G Z (device flat) */
    mpu_plant_accel(0, 0, 16384);
    /* Plant plausible gyro: near-zero rotation */
    mpu_plant_gyro(0, 0, 0);

    mpu6050_cfg_t mpu_cfg = {
        .addr        = MPU6050_ADDR_LOW,
        .accel_range = MPU6050_ACCEL_2G,
        .gyro_range  = MPU6050_GYRO_250DPS,
    };
    mpu6050_err_t mpu_err = mpu6050_init(&mpu_dev, &mpu_i2c, &mpu_cfg);
    ASSERT_EQ(mpu_err, MPU6050_OK);

    /* 2. Servo */
    servo_last_pulse_us = 0;
    servo_pwm_calls = 0;
    servo_cfg_t servo_cfg = {
        .min_pulse_us = SERVO_MIN_PULSE_US,
        .max_pulse_us = SERVO_MAX_PULSE_US,
        .max_angle    = SERVO_MAX_ANGLE,
    };
    servo_err_t servo_err = servo_init(&servo_dev, &servo_pwm, &servo_cfg);
    ASSERT_EQ(servo_err, SERVO_OK);

    /* 3. Stepper */
    stepper_step_count = 0;
    stepper_err_t step_err = stepper_init(&stepper_dev, &stepper_hal,
                                          STEPPER_MAX_SPEED, STEPPER_ACCEL);
    ASSERT_EQ(step_err, STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&stepper_dev), 0);
    ASSERT_EQ(stepper_get_state(&stepper_dev), STEPPER_STATE_IDLE);

    /* 4. PCA9685 */
    pca_mock_reset();
    pca9685_err_t pca_err = pca9685_init(&pca_dev, &pca_i2c,
                                          PCA9685_ADDR_DEFAULT, PCA9685_FREQ);
    ASSERT_EQ(pca_err, PCA9685_OK);

    /* 5. DRV8833 */
    drv_mock_reset();
    drv8833_err_t drv_err = drv8833_init(&drv_dev, &drv_hal, DRV8833_MAX_DUTY);
    ASSERT_EQ(drv_err, DRV8833_OK);

    /* 6. DRV2605 */
    haptic_mock_reset();
    drv2605_err_t hap_err = drv2605_init(&haptic_dev, &haptic_i2c);
    ASSERT_EQ(hap_err, DRV2605_OK);

    printf("  all 6 wheels initialized successfully\n");
}

TEST(test_mpu6050_read_accel_gyro)
{
    /* Plant a 15-degree tilt: ax ~ 0.259G, az ~ 0.966G
     * raw for 2G range (scale=16384): ax = 4240, az = 15826 */
    mpu_plant_accel(4240, 0, 15826);
    mpu_plant_gyro(131, 0, 0);  /* ~1 deg/s around X (scale=131) */

    mpu6050_accel_t accel;
    mpu6050_err_t err = mpu6050_read_accel(&mpu_dev, &accel);
    ASSERT_EQ(err, MPU6050_OK);

    /* Verify accel values: ax ~ 0.259G, ay ~ 0, az ~ 0.966G */
    ASSERT_NEAR(accel.ax_g, 0.259f, 0.01f);
    ASSERT_NEAR(accel.ay_g, 0.0f, 0.01f);
    ASSERT_NEAR(accel.az_g, 0.966f, 0.01f);

    mpu6050_gyro_t gyro;
    err = mpu6050_read_gyro(&mpu_dev, &gyro);
    ASSERT_EQ(err, MPU6050_OK);

    /* Verify gyro: gx ~ 1.0 deg/s */
    ASSERT_NEAR(gyro.gx_dps, 1.0f, 0.1f);
    ASSERT_NEAR(gyro.gy_dps, 0.0f, 0.1f);
    ASSERT_NEAR(gyro.gz_dps, 0.0f, 0.1f);

    /* Verify tilt angle from accel */
    float tilt = compute_tilt_deg(accel.ax_g, accel.az_g);
    ASSERT_NEAR(tilt, 15.0f, 0.5f);

    printf("  accel: ax=%.3fG ay=%.3fG az=%.3fG\n",
           (double)accel.ax_g, (double)accel.ay_g, (double)accel.az_g);
    printf("  gyro:  gx=%.1f gy=%.1f gz=%.1f deg/s\n",
           (double)gyro.gx_dps, (double)gyro.gy_dps, (double)gyro.gz_dps);
    printf("  tilt:  %.1f deg\n", (double)tilt);
}

TEST(test_servo_angle)
{
    servo_pwm_calls = 0;

    /* Set servo to 90 degrees (mid-point) */
    servo_err_t err = servo_set_angle(&servo_dev, 90);
    ASSERT_EQ(err, SERVO_OK);
    ASSERT_EQ(servo_get_angle(&servo_dev), 90);
    ASSERT_EQ(servo_pwm_calls, 1);

    /* Mid-point pulse: 500 + (2000 * 90 / 180) = 500 + 1000 = 1500 us */
    ASSERT_EQ(servo_last_pulse_us, 1500);

    /* Set to 0 degrees */
    err = servo_set_angle(&servo_dev, 0);
    ASSERT_EQ(err, SERVO_OK);
    ASSERT_EQ(servo_get_angle(&servo_dev), 0);
    ASSERT_EQ(servo_last_pulse_us, SERVO_MIN_PULSE_US);

    /* Set to max angle */
    err = servo_set_angle(&servo_dev, SERVO_MAX_ANGLE);
    ASSERT_EQ(err, SERVO_OK);
    ASSERT_EQ(servo_get_angle(&servo_dev), SERVO_MAX_ANGLE);
    ASSERT_EQ(servo_last_pulse_us, SERVO_MAX_PULSE_US);

    printf("  servo angles: 0=%u us, 90=%u us, 180=%u us\n",
           (unsigned)SERVO_MIN_PULSE_US, 1500u,
           (unsigned)SERVO_MAX_PULSE_US);
}

TEST(test_stepper_move)
{
    /* Reset tracking */
    stepper_step_count = 0;
    stepper_set_position(&stepper_dev, 0);

    /* Move 200 steps forward */
    stepper_err_t err = stepper_move(&stepper_dev, 200);
    ASSERT_EQ(err, STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&stepper_dev), 200);
    ASSERT_EQ(stepper_step_count, 200);
    ASSERT_EQ(stepper_get_state(&stepper_dev), STEPPER_STATE_IDLE);

    /* Move 100 steps backward */
    int prev_count = stepper_step_count;
    err = stepper_move(&stepper_dev, -100);
    ASSERT_EQ(err, STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&stepper_dev), 100);
    ASSERT_EQ(stepper_step_count - prev_count, 100);

    /* Move to absolute position 0 */
    prev_count = stepper_step_count;
    err = stepper_move_to(&stepper_dev, 0);
    ASSERT_EQ(err, STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&stepper_dev), 0);
    ASSERT_EQ(stepper_step_count - prev_count, 100);

    printf("  stepper: moved 200 fwd, 100 back, 100 to home; pos=%d\n",
           (int)stepper_get_position(&stepper_dev));
}

TEST(test_pca9685_set_pwm)
{
    /* Set channel 0 to 50% duty */
    pca9685_err_t err = pca9685_set_duty(&pca_dev, 0, 50.0f);
    ASSERT_EQ(err, PCA9685_OK);

    /* Verify register writes: channel 0 base is 0x06
     * 50% of 4095 = ~2048 -> on=0, off=2048 */
    uint16_t off_val = (uint16_t)((uint16_t)pca_regs[0x08] |
                       (uint16_t)((pca_regs[0x09] & 0x0F) << 8));
    ASSERT_NEAR((float)off_val, 2048.0f, 2.0f);

    /* Set channel 5 to servo pulse 1500us (50Hz: period=20000us)
     * count = 1500/20000 * 4096 = ~307 */
    err = pca9685_set_servo_us(&pca_dev, 5, 1500);
    ASSERT_EQ(err, PCA9685_OK);

    /* Channel 5 base = 0x06 + 5*4 = 0x1A */
    uint16_t servo_off = (uint16_t)((uint16_t)pca_regs[0x1C] |
                         (uint16_t)((pca_regs[0x1D] & 0x0F) << 8));
    ASSERT_NEAR((float)servo_off, 307.0f, 5.0f);

    /* All off */
    err = pca9685_all_off(&pca_dev);
    ASSERT_EQ(err, PCA9685_OK);

    printf("  pca9685: ch0 50%% duty (off=%u), ch5 servo 1500us (off=%u)\n",
           (unsigned)off_val, (unsigned)servo_off);
}

TEST(test_drv8833_motor_control)
{
    drv_mock_reset();

    /* Forward at 75% speed on channel A: duty = 750 */
    drv8833_err_t err = drv8833_set_speed(&drv_dev, DRV8833_CH_A, 750);
    ASSERT_EQ(err, DRV8833_OK);
    ASSERT_EQ(drv_pwm_state[0][0], 750);  /* IN1 = 750 (forward) */
    ASSERT_EQ(drv_pwm_state[0][1], 0);    /* IN2 = 0 */

    /* Reverse at 50% speed on channel B: duty = -500 */
    err = drv8833_set_speed(&drv_dev, DRV8833_CH_B, -500);
    ASSERT_EQ(err, DRV8833_OK);
    ASSERT_EQ(drv_pwm_state[1][0], 0);    /* IN1 = 0 */
    ASSERT_EQ(drv_pwm_state[1][1], 500);  /* IN2 = 500 (reverse) */

    /* Brake channel A */
    err = drv8833_brake(&drv_dev, DRV8833_CH_A);
    ASSERT_EQ(err, DRV8833_OK);
    ASSERT_EQ(drv_pwm_state[0][0], DRV8833_MAX_DUTY);
    ASSERT_EQ(drv_pwm_state[0][1], DRV8833_MAX_DUTY);

    /* Coast channel B */
    err = drv8833_coast(&drv_dev, DRV8833_CH_B);
    ASSERT_EQ(err, DRV8833_OK);
    ASSERT_EQ(drv_pwm_state[1][0], 0);
    ASSERT_EQ(drv_pwm_state[1][1], 0);

    /* Sleep/wake */
    drv8833_sleep(&drv_dev);
    ASSERT_TRUE(drv_sleep_state);
    drv8833_wake(&drv_dev);
    ASSERT_FALSE(drv_sleep_state);

    printf("  drv8833: fwd A@750, rev B@500, brake A, coast B, sleep/wake OK\n");
}

TEST(test_drv2605_haptic)
{
    /* Set actuator type to ERM */
    drv2605_err_t err = drv2605_set_actuator(&haptic_dev, DRV2605_ACTUATOR_ERM);
    ASSERT_EQ(err, DRV2605_OK);

    /* Verify feedback register: LRA bit (0x80) should be clear */
    ASSERT_EQ(haptic_regs[0x1A] & 0x80, 0);

    /* Load strong click effect (effect 1) into slot 0 */
    err = drv2605_set_waveform(&haptic_dev, 0, 1);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(haptic_regs[0x04], 1);

    /* Load stop marker into slot 1 */
    err = drv2605_set_waveform(&haptic_dev, 1, 0);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(haptic_regs[0x05], 0);

    /* Trigger playback */
    err = drv2605_go(&haptic_dev);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(haptic_regs[0x0C] & 0x01, 1);

    /* Stop playback */
    err = drv2605_stop(&haptic_dev);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(haptic_regs[0x0C], 0);

    printf("  drv2605: ERM actuator, effect 1 loaded, go/stop OK\n");
}

TEST(test_full_motion_cycle)
{
    printf("\n  --- Full Motion Control Cycle ---\n");

    /* Simulate 5 tilt angles and drive actuators accordingly */
    static const struct {
        int16_t  ax_raw;
        int16_t  az_raw;
        float    expected_tilt;
        uint16_t servo_angle;
        int32_t  stepper_target;
        int16_t  dc_speed;
    } scenarios[] = {
        /* flat:   ax=0,     az=16384  -> 0 deg */
        { 0,     16384,  0.0f,   90,     0,    0 },
        /* tilt +15: ax=4240, az=15826  -> ~15 deg */
        { 4240,  15826,  15.0f,  105,   100,  300 },
        /* tilt +30: ax=8192, az=14189  -> ~30 deg */
        { 8192,  14189,  30.0f,  120,   200,  600 },
        /* tilt -15: ax=-4240, az=15826 -> ~-15 deg */
        { -4240, 15826, -15.0f,  75,   -100, -300 },
        /* tilt -30: ax=-8192, az=14189 -> ~-30 deg */
        { -8192, 14189, -30.0f,  60,   -200, -600 },
    };
    uint32_t n_scenarios = sizeof(scenarios) / sizeof(scenarios[0]);

    printf("  tick  tilt_deg  servo  stepper  dc_speed\n");
    printf("  ----  --------  -----  -------  --------\n");

    for (uint32_t i = 0; i < n_scenarios; i++) {
        /* Plant IMU data */
        mpu_plant_accel(scenarios[i].ax_raw, 0, scenarios[i].az_raw);
        mpu_plant_gyro(0, 0, 0);

        /* Read IMU */
        mpu6050_accel_t accel;
        mpu6050_err_t mpu_err = mpu6050_read_accel(&mpu_dev, &accel);
        ASSERT_EQ(mpu_err, MPU6050_OK);

        /* Compute tilt */
        float tilt = compute_tilt_deg(accel.ax_g, accel.az_g);
        ASSERT_NEAR(tilt, scenarios[i].expected_tilt, 1.0f);

        /* Drive servo based on tilt: map tilt to servo angle
         * servo_angle = 90 + tilt (clamped to 0-180) */
        servo_err_t serr = servo_set_angle(&servo_dev, scenarios[i].servo_angle);
        ASSERT_EQ(serr, SERVO_OK);
        ASSERT_EQ(servo_get_angle(&servo_dev), scenarios[i].servo_angle);

        /* Drive PCA9685 channel 0 with same servo pulse */
        uint32_t pulse = SERVO_MIN_PULSE_US +
            ((uint32_t)(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) *
             scenarios[i].servo_angle) / SERVO_MAX_ANGLE;
        pca9685_err_t perr = pca9685_set_servo_us(&pca_dev, 0, (uint16_t)pulse);
        ASSERT_EQ(perr, PCA9685_OK);

        /* Move stepper to target */
        stepper_err_t sterr = stepper_move_to(&stepper_dev,
                                               scenarios[i].stepper_target);
        ASSERT_EQ(sterr, STEPPER_OK);
        ASSERT_EQ(stepper_get_position(&stepper_dev),
                  scenarios[i].stepper_target);

        /* Drive DC motor proportional to tilt */
        drv8833_err_t derr = drv8833_set_speed(&drv_dev, DRV8833_CH_A,
                                                scenarios[i].dc_speed);
        ASSERT_EQ(derr, DRV8833_OK);

        printf("  %4u  %7.1f   %3u     %4d     %4d\n",
               (unsigned)i, (double)tilt,
               (unsigned)servo_get_angle(&servo_dev),
               (int)stepper_get_position(&stepper_dev),
               (int)scenarios[i].dc_speed);
    }

    /* Haptic feedback on completion: strong click */
    drv2605_err_t herr = drv2605_set_waveform(&haptic_dev, 0, 1);
    ASSERT_EQ(herr, DRV2605_OK);
    herr = drv2605_set_waveform(&haptic_dev, 1, 0);
    ASSERT_EQ(herr, DRV2605_OK);
    herr = drv2605_go(&haptic_dev);
    ASSERT_EQ(herr, DRV2605_OK);

    /* Stop all actuators */
    (void)drv8833_coast(&drv_dev, DRV8833_CH_A);
    (void)drv2605_stop(&haptic_dev);

    printf("  --- cycle complete, haptic feedback fired ---\n");
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("motion_control_adv integration")
    RUN(test_init_all);
    RUN(test_mpu6050_read_accel_gyro);
    RUN(test_servo_angle);
    RUN(test_stepper_move);
    RUN(test_pca9685_set_pwm);
    RUN(test_drv8833_motor_control);
    RUN(test_drv2605_haptic);
    RUN(test_full_motion_cycle);
TEST_MAIN_END()
