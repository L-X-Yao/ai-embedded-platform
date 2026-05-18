// SPDX-License-Identifier: MIT
/*
 * hw_model_ina219.h -- INA219 hardware register model from datasheet
 *
 * Models TI INA219 register behavior:
 * - 6 registers (16-bit big-endian), mapped to pairs in hw_reg_model_t
 * - 0x00 Config (R/W), bit 15 is self-clearing reset
 * - 0x01 Shunt Voltage (read-only, signed 16-bit, LSB = 10uV)
 * - 0x02 Bus Voltage (read-only, bits [15:3], LSB = 4mV)
 * - 0x03 Power (read-only)
 * - 0x04 Current (read-only)
 * - 0x05 Calibration (R/W)
 *
 * Each 16-bit register is stored as two consecutive 8-bit registers
 * (even = MSB, odd = LSB) so the standard hw_model I2C callbacks
 * can serve a 2-byte read_regs call.
 */

#ifndef HW_MODEL_INA219_H
#define HW_MODEL_INA219_H

#include "hw_model.h"

/* --- INA219 register addresses (byte pairs in model) --- */
/* Real reg 0x00 -> model bytes [0x00, 0x01] */
#define INA219_REG_CONFIG_H     0x00
#define INA219_REG_CONFIG_L     0x01
#define INA219_REG_SHUNT_V_H    0x02
#define INA219_REG_SHUNT_V_L    0x03
#define INA219_REG_BUS_V_H      0x04
#define INA219_REG_BUS_V_L      0x05
#define INA219_REG_POWER_H      0x06
#define INA219_REG_POWER_L      0x07
#define INA219_REG_CURRENT_H    0x08
#define INA219_REG_CURRENT_L    0x09
#define INA219_REG_CAL_H        0x0A
#define INA219_REG_CAL_L        0x0B

/* Default config after POR: 0x399F */
#define INA219_CONFIG_DEFAULT_H 0x39
#define INA219_CONFIG_DEFAULT_L 0x9F

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
    int16_t  true_shunt_uv_x10;  /* shunt voltage in 10uV units (signed) */
    uint16_t true_bus_mv_x4;     /* bus voltage in 4mV units, in bits [15:3] format */
    int      reset_count;
} ina219_model_t;

/*
 * Custom I2C callbacks that remap the driver's register address
 * to byte-pair addresses in the model. The INA219 driver uses
 * write_regs/read_regs with a register address 0x00-0x05.
 */
__attribute__((unused))
static bool ina219_model_i2c_write_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg,
                                        const uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    uint8_t base = (uint8_t)(reg * 2);

    /* If writing to config register with RST bit set, just do the reset
     * and ignore the rest of the write data (real hardware behavior). */
    if (reg == 0x00 && len >= 1 && (buf[0] & 0x80)) {
        hw_model__write_reg(m, base, buf[0]);
        return true;
    }

    for (uint16_t i = 0; i < len && (base + i) < HW_MODEL_REG_COUNT; i++) {
        hw_model__write_reg(m, (uint8_t)(base + i), buf[i]);
    }
    return true;
}

__attribute__((unused))
static bool ina219_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                       uint8_t reg,
                                       uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    uint8_t base = (uint8_t)(reg * 2);
    for (uint16_t i = 0; i < len && (base + i) < HW_MODEL_REG_COUNT; i++) {
        buf[i] = m->regs[base + i];
    }
    return true;
}

/* Callback: config register write -- handle reset bit */
static void ina219_model_on_config_h_write(void *user, uint8_t reg,
                                           uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    ina219_model_t *m = (ina219_model_t *)user;
    if (new_val & 0x80) {
        /* RST bit set: reset all registers, self-clear RST */
        hw_model_reset(&m->hw);
        m->reset_count++;
    }
}

/* Compute simulated output registers from calibration + true values */
static void ina219_model_update_outputs(ina219_model_t *m)
{
    /* Shunt voltage register (read-only) */
    m->hw.regs[INA219_REG_SHUNT_V_H] = (uint8_t)(m->true_shunt_uv_x10 >> 8);
    m->hw.regs[INA219_REG_SHUNT_V_L] = (uint8_t)(m->true_shunt_uv_x10 & 0xFF);

    /* Bus voltage register: value in bits [15:3], bits [2:1] unused, bit 0 OVF */
    m->hw.regs[INA219_REG_BUS_V_H] = (uint8_t)(m->true_bus_mv_x4 >> 8);
    m->hw.regs[INA219_REG_BUS_V_L] = (uint8_t)(m->true_bus_mv_x4 & 0xFF);

    /* Current and power depend on calibration register */
    uint16_t cal = (uint16_t)((uint16_t)m->hw.regs[INA219_REG_CAL_H] << 8 |
                               m->hw.regs[INA219_REG_CAL_L]);
    if (cal > 0) {
        int32_t shunt_raw = (int16_t)m->true_shunt_uv_x10;
        int32_t current_raw = (shunt_raw * (int32_t)cal) / 4096;
        int16_t current_reg = (int16_t)current_raw;
        m->hw.regs[INA219_REG_CURRENT_H] = (uint8_t)(current_reg >> 8);
        m->hw.regs[INA219_REG_CURRENT_L] = (uint8_t)(current_reg & 0xFF);

        uint16_t bus_raw = (uint16_t)(m->true_bus_mv_x4 >> 3);
        int32_t power_raw = ((int32_t)(current_reg < 0 ? -current_reg : current_reg)
                             * (int32_t)bus_raw) / 5000;
        uint16_t power_reg = (uint16_t)(power_raw < 0 ? 0 :
                              power_raw > 65535 ? 65535 : power_raw);
        m->hw.regs[INA219_REG_POWER_H] = (uint8_t)(power_reg >> 8);
        m->hw.regs[INA219_REG_POWER_L] = (uint8_t)(power_reg & 0xFF);
    }
}

static void ina219_model_init(ina219_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Config register default: 0x399F */
    hw_model_set_reset_value(&m->hw, INA219_REG_CONFIG_H, INA219_CONFIG_DEFAULT_H);
    hw_model_set_reset_value(&m->hw, INA219_REG_CONFIG_L, INA219_CONFIG_DEFAULT_L);

    /* Shunt, Bus, Power, Current are read-only */
    hw_model_set_readonly(&m->hw, INA219_REG_SHUNT_V_H);
    hw_model_set_readonly(&m->hw, INA219_REG_SHUNT_V_L);
    hw_model_set_readonly(&m->hw, INA219_REG_BUS_V_H);
    hw_model_set_readonly(&m->hw, INA219_REG_BUS_V_L);
    hw_model_set_readonly(&m->hw, INA219_REG_POWER_H);
    hw_model_set_readonly(&m->hw, INA219_REG_POWER_L);
    hw_model_set_readonly(&m->hw, INA219_REG_CURRENT_H);
    hw_model_set_readonly(&m->hw, INA219_REG_CURRENT_L);

    /* Calibration register (R/W) */
    hw_model_set_reset_value(&m->hw, INA219_REG_CAL_H, 0x00);
    hw_model_set_reset_value(&m->hw, INA219_REG_CAL_L, 0x00);

    /* Write callback for reset bit */
    hw_model_set_on_write(&m->hw, INA219_REG_CONFIG_H,
                          ina219_model_on_config_h_write);

    /* Default: 12V bus, 100mA through 0.1 ohm -> 10mV shunt
     * Shunt voltage: 10mV = 1000 x 10uV, so raw = 1000 */
    m->true_shunt_uv_x10 = 1000;
    /* Bus voltage: 12V = 3000 x 4mV, stored in bits [15:3] -> 3000 << 3 */
    m->true_bus_mv_x4 = (uint16_t)(3000u << 3);

    hw_model_reset(&m->hw);
}

/* Helper: set simulated shunt voltage (in 10uV units, signed) */
static void ina219_model_set_shunt(ina219_model_t *m, int16_t raw)
{
    m->true_shunt_uv_x10 = raw;
    ina219_model_update_outputs(m);
}

/* Helper: set simulated bus voltage (in 4mV units, pre-shifted to [15:3]) */
static void ina219_model_set_bus(ina219_model_t *m, uint16_t bus_4mv)
{
    m->true_bus_mv_x4 = (uint16_t)(bus_4mv << 3);
    ina219_model_update_outputs(m);
}

#endif /* HW_MODEL_INA219_H */
