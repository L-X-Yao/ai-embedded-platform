/*
 * hw_model_lsm6dso.h — datasheet-accurate register model for LSM6DSO
 *
 * Models ST LSM6DSO register behavior:
 * - WHO_AM_I 0x6C at register 0x0F (read-only)
 * - CTRL3_C at 0x12 (bit 0 = SW_RESET, bit 6 = BDU)
 * - CTRL1_XL at 0x10, CTRL2_G at 0x11
 * - Accel data 0x28..0x2D (6 bytes, read-only, little-endian)
 * - Gyro  data 0x22..0x27 (6 bytes, read-only, little-endian)
 * - Temp  data 0x20..0x21 (2 bytes, read-only, little-endian)
 */

#ifndef HW_MODEL_LSM6DSO_H
#define HW_MODEL_LSM6DSO_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

typedef struct {
    uint8_t regs[HW_REG_COUNT];
    uint8_t reset_values[HW_REG_COUNT];
    bool    readonly[HW_REG_COUNT];
    int     write_count;
    int     read_count;

    /* Simulated physical environment */
    int16_t true_ax, true_ay, true_az;   /* accel raw */
    int16_t true_gx, true_gy, true_gz;   /* gyro raw */
    int16_t true_temp;                    /* temp raw */
    int     reset_count;
} lsm6dso_model_t;

/* Write sensor data registers (little-endian) */
static void lsm6dso_model_set_data_regs(lsm6dso_model_t *m)
{
    /* Temp: 0x20..0x21 */
    m->regs[0x20] = (uint8_t)((uint16_t)m->true_temp & 0xFF);
    m->regs[0x21] = (uint8_t)((uint16_t)m->true_temp >> 8);
    /* Gyro: 0x22..0x27 */
    m->regs[0x22] = (uint8_t)((uint16_t)m->true_gx & 0xFF);
    m->regs[0x23] = (uint8_t)((uint16_t)m->true_gx >> 8);
    m->regs[0x24] = (uint8_t)((uint16_t)m->true_gy & 0xFF);
    m->regs[0x25] = (uint8_t)((uint16_t)m->true_gy >> 8);
    m->regs[0x26] = (uint8_t)((uint16_t)m->true_gz & 0xFF);
    m->regs[0x27] = (uint8_t)((uint16_t)m->true_gz >> 8);
    /* Accel: 0x28..0x2D */
    m->regs[0x28] = (uint8_t)((uint16_t)m->true_ax & 0xFF);
    m->regs[0x29] = (uint8_t)((uint16_t)m->true_ax >> 8);
    m->regs[0x2A] = (uint8_t)((uint16_t)m->true_ay & 0xFF);
    m->regs[0x2B] = (uint8_t)((uint16_t)m->true_ay >> 8);
    m->regs[0x2C] = (uint8_t)((uint16_t)m->true_az & 0xFF);
    m->regs[0x2D] = (uint8_t)((uint16_t)m->true_az >> 8);
}

static void lsm6dso_model_reset_regs(lsm6dso_model_t *m)
{
    memcpy(m->regs, m->reset_values, HW_REG_COUNT);
    lsm6dso_model_set_data_regs(m);
}

static void lsm6dso_model_init(lsm6dso_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* WHO_AM_I (read-only) */
    m->reset_values[0x0F] = 0x6C;
    m->readonly[0x0F] = true;

    /* Data output registers: temp (0x20..0x21), gyro (0x22..0x27), accel (0x28..0x2D) */
    for (int i = 0x20; i <= 0x2D; i++) {
        m->readonly[i] = true;
    }

    /* Default sensor data: 0g X, 0g Y, +1g Z at 2G range */
    m->true_ax = 0;
    m->true_ay = 0;
    m->true_az = 16393;  /* ~1g at 0.061 mg/LSB */
    m->true_gx = 0;
    m->true_gy = 0;
    m->true_gz = 0;
    m->true_temp = 0;    /* 25C */

    lsm6dso_model_reset_regs(m);
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    lsm6dso_model_t *m = (lsm6dso_model_t *)ctx;
    m->write_count++;

    /* Handle SW_RESET in CTRL3_C */
    if (reg == 0x12 && (val & 0x01)) {
        lsm6dso_model_reset_regs(m);
        m->reset_count++;
        return true;
    }

    if (m->readonly[reg]) {
        return true;  /* silently ignore writes to read-only regs */
    }
    m->regs[reg] = val;
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    lsm6dso_model_t *m = (lsm6dso_model_t *)ctx;
    m->read_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = m->regs[(uint8_t)(reg + i)];
    }
    return true;
}

__attribute__((unused))
static void lsm6dso_model_set_accel(lsm6dso_model_t *m,
                                     int16_t ax, int16_t ay, int16_t az)
{
    m->true_ax = ax;
    m->true_ay = ay;
    m->true_az = az;
    lsm6dso_model_set_data_regs(m);
}

__attribute__((unused))
static void lsm6dso_model_set_gyro(lsm6dso_model_t *m,
                                    int16_t gx, int16_t gy, int16_t gz)
{
    m->true_gx = gx;
    m->true_gy = gy;
    m->true_gz = gz;
    lsm6dso_model_set_data_regs(m);
}

__attribute__((unused))
static void lsm6dso_model_set_temp(lsm6dso_model_t *m, int16_t raw)
{
    m->true_temp = raw;
    lsm6dso_model_set_data_regs(m);
}

#endif /* HW_MODEL_LSM6DSO_H */
