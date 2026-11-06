// SPDX-License-Identifier: MIT
/*
 * hw_model_icm42688.h -- ICM-42688-P hardware register model from datasheet
 *
 * Models TDK ICM-42688-P SPI register behavior:
 * - SPI protocol: bit 7 = 1 for read, 0 for write
 * - WHO_AM_I (0x75) = 0x47 (read-only)
 * - BANK_SEL (0x76) selects register bank. Only bank 0 is modelled: with
 *   another bank selected, the rest of it reads 0 and takes no writes
 * - PWR_MGMT0 (0x4E): GYRO_MODE[3:2] | ACCEL_MODE[1:0]
 * - GYRO_CONFIG0 (0x4F): GYRO_FS_SEL[7:5] | GYRO_ODR[3:0]
 * - ACCEL_CONFIG0 (0x50): ACCEL_FS_SEL[7:5] | ACCEL_ODR[3:0]
 * - DEVICE_CONFIG (0x11): bit 0 = soft_reset_config, which restores the Reset
 *   values of section 14. The wait of 1 ms it needs is not modelled: the HAL
 *   has no delay.
 * - TEMP_DATA (0x1D-0x1E), ACCEL_DATA (0x1F-0x24), GYRO_DATA (0x25-0x2A):
 *   SYNCR, so writes do nothing
 * - INT_STATUS (0x2D): R/C, clears when read
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
#define ICM_REG_DRIVE_CONFIG    0x13
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
#define ICM_REG_INT_STATUS      0x2D
#define ICM_REG_FIFO_DATA       0x30
#define ICM_REG_APEX_DATA3      0x34
#define ICM_REG_INTF_CONFIG0    0x4C
#define ICM_REG_INTF_CONFIG1    0x4D
#define ICM_REG_PWR_MGMT0       0x4E
#define ICM_REG_GYRO_CONFIG0    0x4F
#define ICM_REG_ACCEL_CONFIG0   0x50
#define ICM_REG_GYRO_CONFIG1    0x51
#define ICM_REG_GYRO_ACCEL_CFG0 0x52
#define ICM_REG_ACCEL_CONFIG1   0x53
#define ICM_REG_TMST_CONFIG     0x54
#define ICM_REG_APEX_CONFIG0    0x56
#define ICM_REG_FSYNC_CONFIG    0x62
#define ICM_REG_INT_CONFIG1     0x64
#define ICM_REG_INT_SOURCE0     0x65
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

    /* Section 14.59: REG_BANK_SEL "is accessible from all register banks",
     * and nothing else here is outside bank 0 */
    bool in_bank = m->hw.regs[ICM_REG_BANK_SEL] == 0 ||
                   m->cur_addr == ICM_REG_BANK_SEL;

    /* Data phase */
    if (m->is_read) {
        /* Read: return current register, auto-increment */
        uint8_t val = in_bank ? m->hw.regs[m->cur_addr] : 0x00;
        /* p.69: each INT_STATUS bit "clears to 0 after the register has been
         * read" */
        if (in_bank && m->cur_addr == ICM_REG_INT_STATUS)
            m->hw.regs[ICM_REG_INT_STATUS] = 0x00;
        m->cur_addr = (uint8_t)((m->cur_addr + 1) & 0xFF);
        return val;
    } else {
        /* Write: store value via hw_model (respects masks + callbacks) */
        if (in_bank)
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

    /* Serial IF SYNCR: no W, so the host cannot change them */
    for (uint8_t reg = ICM_REG_TEMP_DATA1; reg <= ICM_REG_GYRO_DATA_Z0; reg++)
        hw_model_set_readonly(&m->hw, reg);

    /* Section 14: the other Reset values that are not 0. INT_STATUS has
     * RESET_DONE_INT set, and 0x06 in the two CONFIG0 registers is 1 kHz. */
    hw_model_set_reset_value(&m->hw, ICM_REG_DRIVE_CONFIG,    0x05);
    hw_model_set_reset_value(&m->hw, ICM_REG_INT_STATUS,      0x10);
    hw_model_set_reset_value(&m->hw, ICM_REG_FIFO_DATA,       0xFF);
    hw_model_set_reset_value(&m->hw, ICM_REG_APEX_DATA3,      0x04);
    hw_model_set_reset_value(&m->hw, ICM_REG_INTF_CONFIG0,    0x30);
    hw_model_set_reset_value(&m->hw, ICM_REG_INTF_CONFIG1,    0x91);
    hw_model_set_reset_value(&m->hw, ICM_REG_GYRO_CONFIG0,    0x06);
    hw_model_set_reset_value(&m->hw, ICM_REG_ACCEL_CONFIG0,   0x06);
    hw_model_set_reset_value(&m->hw, ICM_REG_GYRO_CONFIG1,    0x16);
    hw_model_set_reset_value(&m->hw, ICM_REG_GYRO_ACCEL_CFG0, 0x11);
    hw_model_set_reset_value(&m->hw, ICM_REG_ACCEL_CONFIG1,   0x0D);
    hw_model_set_reset_value(&m->hw, ICM_REG_TMST_CONFIG,     0x23);
    hw_model_set_reset_value(&m->hw, ICM_REG_APEX_CONFIG0,    0x82);
    hw_model_set_reset_value(&m->hw, ICM_REG_FSYNC_CONFIG,    0x10);
    hw_model_set_reset_value(&m->hw, ICM_REG_INT_CONFIG1,     0x10);
    hw_model_set_reset_value(&m->hw, ICM_REG_INT_SOURCE0,     0x10);

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
