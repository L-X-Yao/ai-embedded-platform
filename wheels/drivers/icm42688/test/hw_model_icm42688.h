// SPDX-License-Identifier: MIT
/*
 * hw_model_icm42688.h -- ICM-42688-P hardware register model from datasheet
 *
 * Models TDK ICM-42688-P SPI register behavior:
 * - SPI protocol: bit 7 = 1 for read, 0 for write
 * - WHO_AM_I (0x75) = 0x47 (read-only)
 * - BANK_SEL (0x76) selects register bank (only bank 0 modeled)
 * - PWR_MGMT0 (0x4E): GYRO_MODE[3:2] | ACCEL_MODE[1:0]
 * - GYRO_CONFIG0 (0x4F): GYRO_FS_SEL[6:5] | GYRO_ODR[3:0]
 * - ACCEL_CONFIG0 (0x50): ACCEL_FS_SEL[6:5] | ACCEL_ODR[3:0]
 * - DEVICE_CONFIG (0x11): bit 0 = soft_reset_config
 * - TEMP_DATA (0x1D-0x1E), ACCEL_DATA (0x1F-0x24), GYRO_DATA (0x25-0x2A)
 * - Burst read with auto-increment from any start register
 *
 * Uses hw_reg_model_t for the underlying register file, with custom
 * SPI transaction parsing to model the real chip protocol.
 */

#ifndef HW_MODEL_ICM42688_H
#define HW_MODEL_ICM42688_H

#include "hw_model.h"

/* --- ICM-42688-P register addresses (bank 0) --- */
#define ICM_REG_DEVICE_CONFIG   0x11
#define ICM_REG_INT_CONFIG      0x14
#define ICM_REG_TEMP_DATA1      0x1D
#define ICM_REG_TEMP_DATA0      0x1E
#define ICM_REG_ACCEL_DATA_X1   0x1F
#define ICM_REG_ACCEL_DATA_X0   0x20
#define ICM_REG_ACCEL_DATA_Y1   0x21
#define ICM_REG_ACCEL_DATA_Y0   0x22
#define ICM_REG_ACCEL_DATA_Z1   0x23
#define ICM_REG_ACCEL_DATA_Z0   0x24
#define ICM_REG_GYRO_DATA_X1    0x25
#define ICM_REG_GYRO_DATA_X0    0x26
#define ICM_REG_GYRO_DATA_Y1    0x27
#define ICM_REG_GYRO_DATA_Y0    0x28
#define ICM_REG_GYRO_DATA_Z1    0x29
#define ICM_REG_GYRO_DATA_Z0    0x2A
#define ICM_REG_PWR_MGMT0       0x4E
#define ICM_REG_GYRO_CONFIG0    0x4F
#define ICM_REG_ACCEL_CONFIG0   0x50
#define ICM_REG_WHO_AM_I        0x75
#define ICM_REG_BANK_SEL        0x76

#define ICM_WHO_AM_I_VAL        0x47
#define ICM_SPI_READ_BIT        0x80

/* --- SPI transaction phases --- */
typedef enum {
    ICM_PHASE_ADDR = 0,     /* first byte: reg address + R/W */
    ICM_PHASE_DATA,         /* subsequent bytes: data */
} icm42688_phase_t;

typedef struct {
    hw_reg_model_t    hw;
    /* SPI transaction state */
    icm42688_phase_t  phase;
    uint8_t           cur_addr;     /* current register address */
    bool              is_read;      /* true if read transaction */
    /* Counters */
    int               reset_count;
    int               read_count;   /* completed burst reads */
} icm42688_model_t;

/* --- Write callback: DEVICE_CONFIG soft reset --- */

__attribute__((unused))
static void icm42688_on_device_config_write(void *user, uint8_t reg,
                                            uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    (void)old_val;
    icm42688_model_t *m = (icm42688_model_t *)user;

    if (new_val & 0x01) {
        /* Soft reset: restore all registers to reset values */
        hw_model_reset(&m->hw);
        m->reset_count++;
    }
}

/* --- SPI HAL callbacks --- */

__attribute__((unused))
static void icm42688_model_cs_low(void *ctx)
{
    icm42688_model_t *m = (icm42688_model_t *)ctx;
    m->phase = ICM_PHASE_ADDR;
    m->cur_addr = 0;
    m->is_read = false;
}

__attribute__((unused))
static void icm42688_model_cs_high(void *ctx)
{
    icm42688_model_t *m = (icm42688_model_t *)ctx;
    if (m->is_read) {
        m->read_count++;
    }
}

__attribute__((unused))
static uint8_t icm42688_model_xfer(void *ctx, uint8_t tx)
{
    icm42688_model_t *m = (icm42688_model_t *)ctx;

    if (m->phase == ICM_PHASE_ADDR) {
        /* First byte: register address + R/W bit */
        m->is_read = (tx & ICM_SPI_READ_BIT) != 0;
        m->cur_addr = tx & 0x7F;
        m->phase = ICM_PHASE_DATA;
        return 0x00;    /* status byte */
    }

    /* Data phase */
    if (m->is_read) {
        /* Read: return current register, auto-increment */
        uint8_t val = m->hw.regs[m->cur_addr];
        m->cur_addr = (uint8_t)((m->cur_addr + 1) & 0xFF);
        return val;
    } else {
        /* Write: store value via hw_model (respects masks + callbacks) */
        hw_model__write_reg(&m->hw, m->cur_addr, tx);
        m->cur_addr = (uint8_t)((m->cur_addr + 1) & 0xFF);
        return 0x00;
    }
}

/* --- Model initialization --- */

static void icm42688_model_init(icm42688_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Power-on reset defaults (datasheet) */
    hw_model_set_reset_value(&m->hw, ICM_REG_WHO_AM_I, ICM_WHO_AM_I_VAL);
    hw_model_set_readonly(&m->hw, ICM_REG_WHO_AM_I);

    /* DEVICE_CONFIG: writable, with soft-reset callback */
    hw_model_set_on_write(&m->hw, ICM_REG_DEVICE_CONFIG,
                          icm42688_on_device_config_write);

    /* Sense registers read -32768 until the first sample (DS-000347 Rev 1.6
     * p.60): high bytes reset to 0x80, low bytes to 0x00 (p.65 to p.68) */
    hw_model_set_reset_value(&m->hw, ICM_REG_TEMP_DATA1,    0x80);
    hw_model_set_reset_value(&m->hw, ICM_REG_ACCEL_DATA_X1, 0x80);
    hw_model_set_reset_value(&m->hw, ICM_REG_ACCEL_DATA_Y1, 0x80);
    hw_model_set_reset_value(&m->hw, ICM_REG_ACCEL_DATA_Z1, 0x80);
    hw_model_set_reset_value(&m->hw, ICM_REG_GYRO_DATA_X1,  0x80);
    hw_model_set_reset_value(&m->hw, ICM_REG_GYRO_DATA_Y1,  0x80);
    hw_model_set_reset_value(&m->hw, ICM_REG_GYRO_DATA_Z1,  0x80);

    /* PWR_MGMT0 default: 0x00 (all off) */

    hw_model_reset(&m->hw);
}

/* --- Helper: inject accel data (raw 16-bit signed, big-endian) --- */

__attribute__((unused))
static void icm42688_model_set_accel(icm42688_model_t *m,
                                     int16_t ax, int16_t ay, int16_t az)
{
    m->hw.regs[ICM_REG_ACCEL_DATA_X1] = (uint8_t)((uint16_t)ax >> 8);
    m->hw.regs[ICM_REG_ACCEL_DATA_X0] = (uint8_t)((uint16_t)ax & 0xFF);
    m->hw.regs[ICM_REG_ACCEL_DATA_Y1] = (uint8_t)((uint16_t)ay >> 8);
    m->hw.regs[ICM_REG_ACCEL_DATA_Y0] = (uint8_t)((uint16_t)ay & 0xFF);
    m->hw.regs[ICM_REG_ACCEL_DATA_Z1] = (uint8_t)((uint16_t)az >> 8);
    m->hw.regs[ICM_REG_ACCEL_DATA_Z0] = (uint8_t)((uint16_t)az & 0xFF);
}

/* --- Helper: inject gyro data (raw 16-bit signed, big-endian) --- */

__attribute__((unused))
static void icm42688_model_set_gyro(icm42688_model_t *m,
                                    int16_t gx, int16_t gy, int16_t gz)
{
    m->hw.regs[ICM_REG_GYRO_DATA_X1] = (uint8_t)((uint16_t)gx >> 8);
    m->hw.regs[ICM_REG_GYRO_DATA_X0] = (uint8_t)((uint16_t)gx & 0xFF);
    m->hw.regs[ICM_REG_GYRO_DATA_Y1] = (uint8_t)((uint16_t)gy >> 8);
    m->hw.regs[ICM_REG_GYRO_DATA_Y0] = (uint8_t)((uint16_t)gy & 0xFF);
    m->hw.regs[ICM_REG_GYRO_DATA_Z1] = (uint8_t)((uint16_t)gz >> 8);
    m->hw.regs[ICM_REG_GYRO_DATA_Z0] = (uint8_t)((uint16_t)gz & 0xFF);
}

/* --- Helper: inject temperature data (raw 16-bit signed, big-endian) --- */

__attribute__((unused))
static void icm42688_model_set_temp(icm42688_model_t *m, int16_t raw_temp)
{
    m->hw.regs[ICM_REG_TEMP_DATA1] = (uint8_t)((uint16_t)raw_temp >> 8);
    m->hw.regs[ICM_REG_TEMP_DATA0] = (uint8_t)((uint16_t)raw_temp & 0xFF);
}

#endif /* HW_MODEL_ICM42688_H */
