// SPDX-License-Identifier: MIT
/*
 * hw_model_ina226.h — 16-bit register model for INA226
 *
 * Models INA226's 16-bit big-endian registers as uint16_t array.
 * Each register address maps to one uint16_t. From TI SBOS547C:
 * - POR values and register types, Table 7-1 on p22
 * - RST restores the POR values and self-clears (p23)
 * - Writes to the type R registers are ignored and counted
 * - The Calibration Register has 15 bits (Table 7-11 on p26)
 * - A conversion copies the inputs the MODE bits select into 01h and 02h,
 *   then fills Current and Power by Equations 3 and 4 (p15, p16), which
 *   stay zero with no calibration. Continuous modes convert whenever an
 *   input or CAL changes, triggered modes on a write of Config (p11), and
 *   power-down never (Table 7-6 on p24). Conversion time is not modelled
 * - CVRF rises with a conversion, and falls on a Config write other than
 *   power-down and on a read of 06h (p11). OVF marks an overflow (p27).
 *   Both are read-only, as is AFF, which is not modelled
 *
 * The manual gives the Power Register no sign, so the model uses the
 * magnitude of the current.
 */
#ifndef HW_MODEL_INA226_H
#define HW_MODEL_INA226_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

#define HW_REG_CONFIG  0x00
#define HW_REG_SHUNT   0x01
#define HW_REG_BUS     0x02
#define HW_REG_POWER   0x03
#define HW_REG_CURRENT 0x04
#define HW_REG_CAL     0x05
#define HW_REG_MASK    0x06
#define HW_REG_ALERT   0x07
#define HW_REG_MFR_ID  0xFE
#define HW_REG_DIE_ID  0xFF

#define HW_CONFIG_RST  0x8000u
#define HW_MASK_CVRF   0x0008u
#define HW_MASK_OVF    0x0004u
#define HW_MASK_FIELDS 0xFC03u  /* SOL to CNVR, APOL, LEN */

static uint16_t hw_regs16[HW_REG_COUNT];
static int      hw_write_count;
static int      hw_read_count;
static int      hw_ro_writes;     /* writes to type R or unlisted registers */
static int16_t  hw_in_shunt;      /* what the ADC would measure, raw */
static uint16_t hw_in_bus;

static void hw_model_set_reg16(uint8_t reg, uint16_t val)
{
    hw_regs16[reg] = val;
}

__attribute__((unused))
static uint16_t hw_model_get_reg16(uint8_t reg)
{
    return hw_regs16[reg];
}

static void hw_model_por(void)
{
    memset(hw_regs16, 0, sizeof(hw_regs16));
    /* POR defaults from INA226 datasheet */
    hw_model_set_reg16(HW_REG_CONFIG, 0x4127);
    hw_model_set_reg16(HW_REG_MFR_ID, 0x5449);
    hw_model_set_reg16(HW_REG_DIE_ID, 0x2260);
}

static void hw_model_reset(void)
{
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_ro_writes   = 0;
    hw_in_shunt    = 0;
    hw_in_bus      = 0;
    hw_model_por();
}

static uint16_t hw_model_mode(void)
{
    return hw_regs16[HW_REG_CONFIG] & 0x0007u;
}

static bool hw_model_mode_is_power_down(void)
{
    return (hw_model_mode() & 0x0003u) == 0;
}

static bool hw_model_mode_is_continuous(void)
{
    return (hw_model_mode() & 0x0004u) != 0 && !hw_model_mode_is_power_down();
}

/* One completed conversion */
static void hw_model_convert(void)
{
    if (hw_model_mode_is_power_down()) return;

    if (hw_model_mode() & 0x0001u) hw_regs16[HW_REG_SHUNT] = (uint16_t)hw_in_shunt;
    if (hw_model_mode() & 0x0002u) hw_regs16[HW_REG_BUS] = hw_in_bus;

    /* Equation 3, then Equation 4 from the result */
    int32_t current = (int32_t)(int16_t)hw_regs16[HW_REG_SHUNT] *
                      (int32_t)hw_regs16[HW_REG_CAL] / 2048;
    uint32_t magnitude = (uint32_t)(current < 0 ? -current : current);
    uint32_t power = magnitude * hw_regs16[HW_REG_BUS] / 20000u;

    if (current > INT16_MAX || current < INT16_MIN || power > UINT16_MAX) {
        hw_regs16[HW_REG_MASK] |= HW_MASK_OVF;
    }
    hw_regs16[HW_REG_CURRENT] = (uint16_t)(current & 0xFFFF);
    hw_regs16[HW_REG_POWER] = (uint16_t)(power & 0xFFFFu);
    hw_regs16[HW_REG_MASK] |= HW_MASK_CVRF;
}

static void hw_model_write_config(uint16_t val)
{
    if (val & HW_CONFIG_RST) {
        hw_model_por();
        return;
    }
    hw_regs16[HW_REG_CONFIG] = val;
    if (hw_model_mode_is_power_down()) return;
    hw_regs16[HW_REG_MASK] &= (uint16_t)~HW_MASK_CVRF;
    hw_model_convert();
}

__attribute__((unused))
static bool hw_model_write_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    if (len < 2) return true;

    uint16_t val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    switch (reg) {
    case HW_REG_CONFIG:
        hw_model_write_config(val);
        break;
    case HW_REG_CAL:
        hw_regs16[HW_REG_CAL] = val & 0x7FFFu;
        if (hw_model_mode_is_continuous()) hw_model_convert();
        break;
    case HW_REG_MASK:
        hw_regs16[HW_REG_MASK] = (uint16_t)((hw_regs16[HW_REG_MASK] &
                                             (uint16_t)~HW_MASK_FIELDS) |
                                            (val & HW_MASK_FIELDS));
        break;
    case HW_REG_ALERT:
        hw_regs16[HW_REG_ALERT] = val;
        break;
    default:
        hw_ro_writes++;
        break;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    if (len >= 2) {
        uint16_t val = hw_regs16[reg];
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
        if (reg == HW_REG_MASK) {
            hw_regs16[HW_REG_MASK] &= (uint16_t)~HW_MASK_CVRF;
        }
    }
    return true;
}

/* Inject a raw shunt reading, as the ADC would produce it */
__attribute__((unused))
static void hw_model_inject_shunt_raw(int16_t raw)
{
    hw_in_shunt = raw;
    if (hw_model_mode_is_continuous()) hw_model_convert();
}

/* Inject a raw bus reading, as the ADC would produce it */
__attribute__((unused))
static void hw_model_inject_bus_raw(uint16_t raw)
{
    hw_in_bus = raw;
    if (hw_model_mode_is_continuous()) hw_model_convert();
}

/* Inject a known shunt voltage for testing.
 * uv is in microvolts; LSB = 2.5uV */
__attribute__((unused))
static void hw_model_inject_shunt_uv(int32_t uv)
{
    hw_model_inject_shunt_raw((int16_t)(uv * 10 / 25));
}

/* Inject a known bus voltage for testing.
 * mv is in millivolts; LSB = 1.25mV */
__attribute__((unused))
static void hw_model_inject_bus_mv(uint32_t mv)
{
    hw_model_inject_bus_raw((uint16_t)(mv * 100u / 125u));
}

#endif /* HW_MODEL_INA226_H */
