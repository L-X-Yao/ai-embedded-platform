// SPDX-License-Identifier: MIT
/*
 * hw_model_tmp117.h — datasheet-accurate 16-bit register model for TMP117
 *
 * All TMP117 registers are 16-bit, MSB first.
 * Device ID: 0x0117 at register 0x0F.
 * Temperature: signed 16-bit at register 0x00, resolution 0.0078125°C/LSB.
 * Registers 0x00 and 0x0F, and the R bits of 0x01 and 0x04, ignore writes.
 * Soft_Reset reloads the reset values and reads back 0, and for its 2 ms the
 * limit and configuration registers ignore writes, EEPROM_Busy is set and no
 * conversion runs. Time passes in hw_model_delay_ms.
 * A conversion sets Data_Ready, and reading 0x00 or 0x01 clears it.
 */
#ifndef HW_MODEL_TMP117_H
#define HW_MODEL_TMP117_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 16

/* Configuration register, Figure 7-14 p27 */
#define HW_CFG_DATA_READY   0x2000u
#define HW_CFG_EEPROM_BUSY  0x1000u
#define HW_CFG_MOD_MASK     0x0C00u
#define HW_CFG_MOD_SD       0x0400u
#define HW_CFG_MOD_CC_ALT   0x0800u  /* "Same as 00 (reads back = 00)" */
#define HW_CFG_SOFT_RESET   0x0002u
#define HW_CFG_WRITABLE     0x0FFCu  /* MOD, CONV, AVG, T/nA, POL, DR/Alert */

/* EEPROM unlock register, Figure 7-17 p30 */
#define HW_UL_EUN           0x8000u
#define HW_UL_EEPROM_BUSY   0x4000u

#define HW_SOFT_RESET_MS    2u       /* p28: "a duration of 2 ms" */

static uint16_t hw_regs16[HW_REG_COUNT];
static int      hw_write_count;
static int      hw_read_count;
static int      hw_reset_count;
static uint32_t hw_reset_left_ms;    /* soft reset still running */

/* p25 Table 7-3. The EEPROM1-3 reset values are per part, left at 0 here. */
static void hw_model_load_reset_values(void)
{
    memset(hw_regs16, 0, sizeof(hw_regs16));
    hw_regs16[0x00] = 0x8000;  /* temperature: POR default */
    hw_regs16[0x01] = 0x0220;  /* config: continuous, 1 s cycle, 8 averages */
    hw_regs16[0x02] = 0x6000;  /* high limit */
    hw_regs16[0x03] = 0x8000;  /* low limit */
    hw_regs16[0x0F] = 0x0117;  /* device ID */
}

static void hw_model_reset(void)
{
    hw_model_load_reset_values();
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_reset_count = 0;
    hw_reset_left_ms = 0;
}

/* p28: Soft_Reset "triggers software reset with a duration of 2 ms". p18: the
 * reset values load from EEPROM, and EEPROM_Busy is set meanwhile. */
static void hw_model_soft_reset(void)
{
    hw_model_load_reset_values();
    hw_regs16[0x01] |= HW_CFG_EEPROM_BUSY;
    hw_regs16[0x04] |= HW_UL_EEPROM_BUSY;
    hw_reset_left_ms = HW_SOFT_RESET_MS;
    hw_reset_count++;
}

/* Let simulated time pass */
__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    if (hw_reset_left_ms == 0u) return;
    hw_reset_left_ms -= (ms < hw_reset_left_ms) ? ms : hw_reset_left_ms;
    if (hw_reset_left_ms == 0u) {
        hw_regs16[0x01] &= (uint16_t)~HW_CFG_EEPROM_BUSY;
        hw_regs16[0x04] &= (uint16_t)~HW_UL_EEPROM_BUSY;
    }
}

__attribute__((unused))
static bool hw_model_write_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    if (reg >= HW_REG_COUNT || len != 2)
        return false;

    uint16_t val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);

    /* p25 Table 7-3: type R */
    if (reg == 0x00 || reg == 0x0F)
        return true;

    /* p18: "Any I2C writes performed during this initial POR period to the
     * limit registers or the configuration register are ignored." */
    if (hw_reset_left_ms > 0u && reg >= 0x01 && reg <= 0x03)
        return true;

    if (reg == 0x01) {
        if (val & HW_CFG_SOFT_RESET) {
            hw_model_soft_reset();  /* p28: "will always read back 0" */
            return true;
        }
        val &= HW_CFG_WRITABLE;
        if ((val & HW_CFG_MOD_MASK) == HW_CFG_MOD_CC_ALT)
            val &= (uint16_t)~HW_CFG_MOD_MASK;
        hw_regs16[0x01] = (uint16_t)((hw_regs16[0x01] & (uint16_t)~HW_CFG_WRITABLE) | val);
        return true;
    }

    if (reg == 0x04) {
        hw_regs16[0x04] = (uint16_t)((hw_regs16[0x04] & (uint16_t)~HW_UL_EUN) |
                                     (val & HW_UL_EUN));
        return true;
    }

    hw_regs16[reg] = val;
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    if (reg >= HW_REG_COUNT || len != 2)
        return false;
    buf[0] = (uint8_t)(hw_regs16[reg] >> 8);
    buf[1] = (uint8_t)(hw_regs16[reg] & 0xFF);
    /* p27: "Every time the temperature register or configuration register is
     * read, this bit is cleared." */
    if (reg == 0x00 || reg == 0x01)
        hw_regs16[0x01] &= (uint16_t)~HW_CFG_DATA_READY;
    return true;
}

/* End of one conversion. None runs in shutdown (p14), and p12 has the part
 * power up "before conversions can begin". */
static void hw_model_inject_temperature(float temp_c)
{
    if ((hw_regs16[0x01] & HW_CFG_MOD_MASK) == HW_CFG_MOD_SD)
        return;
    if (hw_reset_left_ms > 0u)
        return;
    int16_t raw = (int16_t)(temp_c / 0.0078125f);
    hw_regs16[0x00] = (uint16_t)raw;
    hw_regs16[0x01] |= HW_CFG_DATA_READY;
}

#endif /* HW_MODEL_TMP117_H */
