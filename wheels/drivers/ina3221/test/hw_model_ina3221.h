// SPDX-License-Identifier: MIT
/*
 * hw_model_ina3221.h -- INA3221 hardware register model from datasheet
 *
 * Models TI INA3221 register behavior using a uint16_t[256] array.
 * Each entry represents one 16-bit register at the corresponding address.
 * Custom I2C callbacks handle the big-endian byte split that the driver
 * expects from read_regs / write_regs.
 *
 * Key registers:
 * - 0x01/0x03/0x05 Shunt voltage (read-only, 13-bit signed, 40 uV/LSB)
 * - 0x02/0x04/0x06 Bus voltage (read-only, 13-bit signed, 8 mV/LSB)
 * - 0x07-0x0C alert limits, 0x0E sum limit, 0x0F Mask/Enable, 0x10/0x11
 *   power-valid limits, 0x0D Shunt-Voltage Sum (read-only)
 * - 0x00 Configuration: a write with RST (bit 15) is a power-on reset
 * - 0xFE Manufacturer ID = 0x5449 ("TI")
 * - 0xFF Die ID = 0x3220
 */

#ifndef HW_MODEL_INA3221_H
#define HW_MODEL_INA3221_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define INA3221_MODEL_REG_COUNT 256

/* Register addresses */
#define INA3221_REG_CONFIG        0x00u
#define INA3221_REG_CH1_SHUNT_V   0x01u
#define INA3221_REG_CH1_BUS_V     0x02u
#define INA3221_REG_CH2_SHUNT_V   0x03u
#define INA3221_REG_CH2_BUS_V     0x04u
#define INA3221_REG_CH3_SHUNT_V   0x05u
#define INA3221_REG_CH3_BUS_V     0x06u
#define INA3221_REG_CH1_CRITICAL  0x07u
#define INA3221_REG_CH3_WARNING   0x0Cu
#define INA3221_REG_SHUNT_SUM     0x0Du
#define INA3221_REG_SUM_LIMIT     0x0Eu
#define INA3221_REG_MASK_ENABLE   0x0Fu
#define INA3221_REG_PV_UPPER      0x10u
#define INA3221_REG_PV_LOWER      0x11u
#define INA3221_REG_MFR_ID        0xFEu
#define INA3221_REG_DIE_ID        0xFFu

/* POR defaults */
#define INA3221_MFR_ID_DEFAULT    0x5449u
#define INA3221_DIE_ID_DEFAULT    0x3220u
#define INA3221_CONFIG_DEFAULT    0x7127u  /* datasheet POR config */
#define INA3221_LIMIT_DEFAULT     0x7FF8u
#define INA3221_SUM_LIMIT_DEFAULT 0x7FFEu
#define INA3221_MASK_DEFAULT      0x0002u
#define INA3221_PV_UPPER_DEFAULT  0x2710u
#define INA3221_PV_LOWER_DEFAULT  0x2328u

#define INA3221_CONFIG_RST        0x8000u

typedef struct {
    uint16_t regs[INA3221_MODEL_REG_COUNT];
    bool     readonly[INA3221_MODEL_REG_COUNT];
    int      write_count;
    int      read_count;
} ina3221_model_t;

/* p.24 Table 8-1, the POWER-ON RESET column */
static void ina3221_model_por(ina3221_model_t *m)
{
    memset(m->regs, 0, sizeof(m->regs));
    m->regs[INA3221_REG_CONFIG] = INA3221_CONFIG_DEFAULT;
    for (unsigned reg = INA3221_REG_CH1_CRITICAL;
         reg <= INA3221_REG_CH3_WARNING; reg++) {
        m->regs[reg] = INA3221_LIMIT_DEFAULT;
    }
    m->regs[INA3221_REG_SUM_LIMIT]   = INA3221_SUM_LIMIT_DEFAULT;
    m->regs[INA3221_REG_MASK_ENABLE] = INA3221_MASK_DEFAULT;
    m->regs[INA3221_REG_PV_UPPER]    = INA3221_PV_UPPER_DEFAULT;
    m->regs[INA3221_REG_PV_LOWER]    = INA3221_PV_LOWER_DEFAULT;
    m->regs[INA3221_REG_MFR_ID]      = INA3221_MFR_ID_DEFAULT;
    m->regs[INA3221_REG_DIE_ID]      = INA3221_DIE_ID_DEFAULT;
}

static void ina3221_model_init(ina3221_model_t *m)
{
    memset(m, 0, sizeof(*m));
    ina3221_model_por(m);

    /* Mark read-only registers */
    m->readonly[INA3221_REG_CH1_SHUNT_V] = true;
    m->readonly[INA3221_REG_CH1_BUS_V]   = true;
    m->readonly[INA3221_REG_CH2_SHUNT_V] = true;
    m->readonly[INA3221_REG_CH2_BUS_V]   = true;
    m->readonly[INA3221_REG_CH3_SHUNT_V] = true;
    m->readonly[INA3221_REG_CH3_BUS_V]   = true;
    m->readonly[INA3221_REG_SHUNT_SUM]   = true;
    m->readonly[INA3221_REG_MFR_ID]      = true;
    m->readonly[INA3221_REG_DIE_ID]      = true;
}

/*
 * I2C read callback: driver reads 2 bytes from register `reg`.
 * We split the 16-bit value into big-endian byte pair.
 */
__attribute__((unused))
static bool ina3221_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg,
                                        uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    ina3221_model_t *m = (ina3221_model_t *)ctx;
    m->read_count++;

    /* The driver always reads 2 bytes (one 16-bit register) */
    if (len >= 2) {
        uint16_t val = m->regs[reg];
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
    }
    /* If len > 2, fill remaining with next register(s) */
    for (uint16_t i = 2; i + 1 < len; i += 2) {
        uint8_t next_reg = (uint8_t)(reg + (i / 2));
        uint16_t val = m->regs[next_reg];
        buf[i]     = (uint8_t)(val >> 8);
        buf[i + 1] = (uint8_t)(val & 0xFF);
    }
    return true;
}

/*
 * I2C write callback: driver writes 2 bytes to register `reg`.
 * We assemble the big-endian byte pair into a 16-bit value.
 */
__attribute__((unused))
static bool ina3221_model_i2c_write_regs(void *ctx, uint8_t dev_addr,
                                         uint8_t reg,
                                         const uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    ina3221_model_t *m = (ina3221_model_t *)ctx;
    m->write_count++;

    if (len >= 2 && !m->readonly[reg]) {
        uint16_t val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);

        /* p.26 "This bit resets all registers to default values and
         * self-clears." */
        if (reg == INA3221_REG_CONFIG && (val & INA3221_CONFIG_RST)) {
            ina3221_model_por(m);
        } else {
            m->regs[reg] = val;
        }
    }
    return true;
}

/*
 * Inject shunt voltage for a channel (0-2).
 * shunt_uv: shunt voltage in microvolts (float).
 * Encoded as 13-bit signed value in bits[15:3], 40 uV/LSB.
 */
__attribute__((unused))
static void ina3221_model_inject_shunt(ina3221_model_t *m,
                                       int channel, float shunt_uv)
{
    static const uint8_t shunt_regs[3] = {
        INA3221_REG_CH1_SHUNT_V,
        INA3221_REG_CH2_SHUNT_V,
        INA3221_REG_CH3_SHUNT_V,
    };

    int16_t counts = (int16_t)(shunt_uv / 40.0f);
    /* Register format: signed value in bits[15:3], lower 3 bits zero.
     * Shift as unsigned: left-shifting a negative int is undefined. */
    uint16_t regval = (uint16_t)((uint16_t)counts << 3);

    m->regs[shunt_regs[channel]] = regval;
}

/*
 * Inject bus voltage for a channel (0-2).
 * bus_mv: bus voltage in millivolts (float).
 * Encoded as 13-bit signed value in bits[15:3], 8 mV/LSB.
 */
__attribute__((unused))
static void ina3221_model_inject_bus(ina3221_model_t *m,
                                     int channel, float bus_mv)
{
    static const uint8_t bus_regs[3] = {
        INA3221_REG_CH1_BUS_V,
        INA3221_REG_CH2_BUS_V,
        INA3221_REG_CH3_BUS_V,
    };

    int16_t counts = (int16_t)(bus_mv / 8.0f);
    uint16_t regval = (uint16_t)((uint16_t)counts << 3);

    m->regs[bus_regs[channel]] = regval;
}

#endif /* HW_MODEL_INA3221_H */
