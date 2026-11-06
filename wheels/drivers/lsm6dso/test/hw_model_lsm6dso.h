// SPDX-License-Identifier: MIT
/*
 * hw_model_lsm6dso.h — datasheet-accurate register model for LSM6DSO
 *
 * Models ST LSM6DSO register behavior:
 * - WHO_AM_I 0x6C at register 0x0F (read-only)
 * - CTRL3_C at 0x12, reset 0x04 (bit 0 = SW_RESET and bit 7 = BOOT clear
 *   themselves, bit 2 = IF_INC, bit 6 = BDU)
 * - CTRL1_XL at 0x10, CTRL2_G at 0x11; ODR 0 is power-down, so no sample
 *   reaches the output registers
 * - STATUS_REG at 0x1E (read-only): a sample sets XLDA, GDA or TDA, reading
 *   the high byte of that output clears it
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

/* STATUS_REG (1Eh) flags */
#define LSM6DSO_MODEL_XLDA 0x01u
#define LSM6DSO_MODEL_GDA  0x02u
#define LSM6DSO_MODEL_TDA  0x04u

/* Each sensor leaves power-down through its own ODR field (p27) */
static bool lsm6dso_model_accel_on(const lsm6dso_model_t *m)
{
    return (m->regs[0x10] >> 4) != 0;
}

static bool lsm6dso_model_gyro_on(const lsm6dso_model_t *m)
{
    return (m->regs[0x11] >> 4) != 0;
}

/* Put one little-endian word into the output registers */
static void lsm6dso_model_put16(lsm6dso_model_t *m, uint8_t reg, int16_t val)
{
    m->regs[reg]     = (uint8_t)((uint16_t)val & 0xFF);
    m->regs[reg + 1] = (uint8_t)((uint16_t)val >> 8);
}

/* Reading "one of the high parts of the output data" resets the flag (p64
 * Table 81, the same flags seen from the auxiliary SPI) */
static void lsm6dso_model_on_read(lsm6dso_model_t *m, uint8_t reg)
{
    if (reg == 0x21)
        m->regs[0x1E] &= (uint8_t)~LSM6DSO_MODEL_TDA;
    else if (reg == 0x23 || reg == 0x25 || reg == 0x27)
        m->regs[0x1E] &= (uint8_t)~LSM6DSO_MODEL_GDA;
    else if (reg == 0x29 || reg == 0x2B || reg == 0x2D)
        m->regs[0x1E] &= (uint8_t)~LSM6DSO_MODEL_XLDA;
}

static void lsm6dso_model_reset_regs(lsm6dso_model_t *m)
{
    memcpy(m->regs, m->reset_values, HW_REG_COUNT);
}

static void lsm6dso_model_init(lsm6dso_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* WHO_AM_I (read-only) */
    m->reset_values[0x0F] = 0x6C;
    m->readonly[0x0F] = true;

    /* CTRL3_C resets with IF_INC (bit 2) set */
    m->reset_values[0x12] = 0x04;

    /* p40 Table 20: "CTRL9_XL RW 18 00011000 11100000" */
    m->reset_values[0x18] = 0xE0;

    /* STATUS_REG (read-only) */
    m->readonly[0x1E] = true;

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
    if (reg == 0x12) {
        m->regs[reg] &= 0x7Fu;  /* p52: BOOT "is automatically cleared" */
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    lsm6dso_model_t *m = (lsm6dso_model_t *)ctx;
    m->read_count++;

    /* With IF_INC clear, a multi-byte read stays on one register */
    bool inc = (m->regs[0x12] & 0x04) != 0;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t addr = (uint8_t)(reg + (inc ? i : 0));
        buf[i] = m->regs[addr];
        lsm6dso_model_on_read(m, addr);
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
    if (lsm6dso_model_accel_on(m)) {
        lsm6dso_model_put16(m, 0x28, ax);
        lsm6dso_model_put16(m, 0x2A, ay);
        lsm6dso_model_put16(m, 0x2C, az);
        m->regs[0x1E] |= LSM6DSO_MODEL_XLDA;
    }
}

__attribute__((unused))
static void lsm6dso_model_set_gyro(lsm6dso_model_t *m,
                                    int16_t gx, int16_t gy, int16_t gz)
{
    m->true_gx = gx;
    m->true_gy = gy;
    m->true_gz = gz;
    if (lsm6dso_model_gyro_on(m)) {
        lsm6dso_model_put16(m, 0x22, gx);
        lsm6dso_model_put16(m, 0x24, gy);
        lsm6dso_model_put16(m, 0x26, gz);
        m->regs[0x1E] |= LSM6DSO_MODEL_GDA;
    }
}

__attribute__((unused))
static void lsm6dso_model_set_temp(lsm6dso_model_t *m, int16_t raw)
{
    m->true_temp = raw;
    /* Temperature has no ODR of its own: it needs one of the sensors on */
    if (lsm6dso_model_accel_on(m) || lsm6dso_model_gyro_on(m)) {
        lsm6dso_model_put16(m, 0x20, raw);
        m->regs[0x1E] |= LSM6DSO_MODEL_TDA;
    }
}

#endif /* HW_MODEL_LSM6DSO_H */
