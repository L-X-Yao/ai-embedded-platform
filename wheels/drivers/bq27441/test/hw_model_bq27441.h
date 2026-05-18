// SPDX-License-Identifier: MIT
/*
 * hw_model_bq27441.h — BQ27441-G1 hardware register model from datasheet
 *
 * Models TI SLUUAC9A behavior:
 * - 16-bit little-endian registers (each stored as two bytes in hw_reg_model_t)
 * - CONTROL register (0x00-0x01): sub-command interface
 *   - Write DEVICE_TYPE (0x0001) -> read back 0x0421
 *   - Write SET_CFGUPDATE (0x0013) -> set FLAGS.CFGUPMODE
 *   - Write SOFT_RESET (0x0042) -> clear FLAGS.CFGUPMODE
 *   - Write SEALED (0x0020) -> seal the device
 * - Standard data registers (read-only, auto-updated from model state)
 * - FLAGS register with CFGUPMODE bit
 * - DESIGN_CAPACITY register (writable in config mode)
 *
 * Unlike INA219's big-endian, BQ27441 is little-endian: reg N = LSB, reg N+1 = MSB.
 */

#ifndef HW_MODEL_BQ27441_H
#define HW_MODEL_BQ27441_H

#include "hw_model.h"

/* --- BQ27441 register byte addresses in model --- */
#define BQ_REG_CTRL_L       0x00u
#define BQ_REG_CTRL_H       0x01u
#define BQ_REG_TEMP_L       0x02u
#define BQ_REG_TEMP_H       0x03u
#define BQ_REG_VOLT_L       0x04u
#define BQ_REG_VOLT_H       0x05u
#define BQ_REG_FLAGS_L      0x06u
#define BQ_REG_FLAGS_H      0x07u
#define BQ_REG_REMCAP_L     0x0Cu
#define BQ_REG_REMCAP_H     0x0Du
#define BQ_REG_FULLCAP_L    0x0Eu
#define BQ_REG_FULLCAP_H    0x0Fu
#define BQ_REG_AVGCUR_L     0x10u
#define BQ_REG_AVGCUR_H     0x11u
#define BQ_REG_AVGPWR_L     0x18u
#define BQ_REG_AVGPWR_H     0x19u
#define BQ_REG_SOC_L        0x1Cu
#define BQ_REG_SOC_H        0x1Du
#define BQ_REG_DCAP_L       0x3Cu
#define BQ_REG_DCAP_H       0x3Du

/* --- FLAGS bits --- */
#define BQ_FLAG_CFGUPMODE   (1u << 4)

/* --- Control sub-commands --- */
#define BQ_CTRL_DEVICE_TYPE   0x0001u
#define BQ_CTRL_SET_CFGUPDATE 0x0013u
#define BQ_CTRL_SEALED        0x0020u
#define BQ_CTRL_SOFT_RESET    0x0042u

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
    uint16_t device_type;       /* response to DEVICE_TYPE sub-cmd */
    uint16_t voltage_mv;        /* battery voltage */
    int16_t  current_ma;        /* average current (negative = discharge) */
    uint16_t soc_pct;           /* state of charge 0-100 */
    uint16_t temp_01k;          /* temperature in 0.1 K */
    uint16_t remaining_mah;     /* remaining capacity */
    uint16_t full_charge_mah;   /* full charge capacity */
    int16_t  avg_power_mw;      /* average power */
    uint16_t design_cap_mah;    /* design capacity */
    bool     cfgupdate_mode;    /* config update mode active */
    bool     sealed;            /* device is sealed */
} bq27441_model_t;

/* --- helpers: set/get 16-bit LE in model regs --- */
static void bq_model_set_le16(hw_reg_model_t *hw, uint8_t reg, uint16_t val)
{
    hw->regs[reg]     = (uint8_t)(val & 0xFFu);
    hw->regs[reg + 1] = (uint8_t)(val >> 8);
}

__attribute__((unused))
static uint16_t bq_model_get_le16(const hw_reg_model_t *hw, uint8_t reg)
{
    return (uint16_t)((uint16_t)hw->regs[reg + 1] << 8 | hw->regs[reg]);
}

/* Update read-only data registers from model state */
static void bq_model_update_outputs(bq27441_model_t *m)
{
    bq_model_set_le16(&m->hw, BQ_REG_VOLT_L,    m->voltage_mv);
    bq_model_set_le16(&m->hw, BQ_REG_AVGCUR_L,  (uint16_t)m->current_ma);
    bq_model_set_le16(&m->hw, BQ_REG_SOC_L,     m->soc_pct);
    bq_model_set_le16(&m->hw, BQ_REG_TEMP_L,    m->temp_01k);
    bq_model_set_le16(&m->hw, BQ_REG_REMCAP_L,  m->remaining_mah);
    bq_model_set_le16(&m->hw, BQ_REG_FULLCAP_L, m->full_charge_mah);
    bq_model_set_le16(&m->hw, BQ_REG_AVGPWR_L,  (uint16_t)m->avg_power_mw);

    /* FLAGS */
    uint16_t flags = 0;
    if (m->cfgupdate_mode) flags |= BQ_FLAG_CFGUPMODE;
    bq_model_set_le16(&m->hw, BQ_REG_FLAGS_L, flags);

    /* Design capacity (readable) */
    bq_model_set_le16(&m->hw, BQ_REG_DCAP_L, m->design_cap_mah);
}

/*
 * Custom I2C write_regs callback: handles CONTROL sub-commands.
 * The BQ27441 driver writes 2-byte LE data to register addresses.
 */
__attribute__((unused))
static bool bq_model_i2c_write_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, const uint8_t *buf,
                                     uint16_t len)
{
    (void)dev_addr;
    bq27441_model_t *m = (bq27441_model_t *)ctx;

    if (reg == BQ_REG_CTRL_L && len == 2) {
        /* CONTROL sub-command write */
        uint16_t subcmd = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);

        switch (subcmd) {
        case BQ_CTRL_DEVICE_TYPE:
            /* Next read of CONTROL returns device type */
            bq_model_set_le16(&m->hw, BQ_REG_CTRL_L, m->device_type);
            break;
        case BQ_CTRL_SET_CFGUPDATE:
            m->cfgupdate_mode = true;
            bq_model_update_outputs(m);
            break;
        case BQ_CTRL_SOFT_RESET:
            m->cfgupdate_mode = false;
            bq_model_update_outputs(m);
            break;
        case BQ_CTRL_SEALED:
            m->sealed = true;
            break;
        default:
            /* Store the sub-command as-is */
            bq_model_set_le16(&m->hw, BQ_REG_CTRL_L, subcmd);
            break;
        }
        return true;
    }

    /* Design capacity write (only in config mode in real hw, but we
     * model it as always writable for simplicity) */
    if (reg == BQ_REG_DCAP_L && len == 2) {
        m->design_cap_mah = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
        bq_model_update_outputs(m);
        return true;
    }

    /* Other registers: use standard model write */
    for (uint16_t i = 0; i < len && ((unsigned)reg + i) < HW_MODEL_REG_COUNT; i++) {
        hw_model__write_reg(&m->hw, (uint8_t)(reg + i), buf[i]);
    }
    return true;
}

__attribute__((unused))
static bool bq_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    bq27441_model_t *m = (bq27441_model_t *)ctx;
    for (uint16_t i = 0; i < len && ((unsigned)reg + i) < HW_MODEL_REG_COUNT; i++) {
        buf[i] = m->hw.regs[reg + i];
    }
    return true;
}

/* delay_ms callback (no-op for model) */
__attribute__((unused))
static void bq_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static void bq_model_init(bq27441_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Defaults: typical single-cell Li-Ion battery */
    m->device_type    = 0x0421u;
    m->voltage_mv     = 3700;
    m->current_ma     = -150;
    m->soc_pct        = 75;
    m->temp_01k       = 2981;  /* 25.0 degC = 298.1 K */
    m->remaining_mah  = 1500;
    m->full_charge_mah = 2000;
    m->avg_power_mw   = -555;
    m->design_cap_mah = 2000;
    m->cfgupdate_mode = false;
    m->sealed         = true;

    /* Mark data registers as read-only in the model */
    hw_model_set_readonly(&m->hw, BQ_REG_TEMP_L);
    hw_model_set_readonly(&m->hw, BQ_REG_TEMP_H);
    hw_model_set_readonly(&m->hw, BQ_REG_VOLT_L);
    hw_model_set_readonly(&m->hw, BQ_REG_VOLT_H);
    hw_model_set_readonly(&m->hw, BQ_REG_FLAGS_L);
    hw_model_set_readonly(&m->hw, BQ_REG_FLAGS_H);
    hw_model_set_readonly(&m->hw, BQ_REG_REMCAP_L);
    hw_model_set_readonly(&m->hw, BQ_REG_REMCAP_H);
    hw_model_set_readonly(&m->hw, BQ_REG_FULLCAP_L);
    hw_model_set_readonly(&m->hw, BQ_REG_FULLCAP_H);
    hw_model_set_readonly(&m->hw, BQ_REG_AVGCUR_L);
    hw_model_set_readonly(&m->hw, BQ_REG_AVGCUR_H);
    hw_model_set_readonly(&m->hw, BQ_REG_AVGPWR_L);
    hw_model_set_readonly(&m->hw, BQ_REG_AVGPWR_H);
    hw_model_set_readonly(&m->hw, BQ_REG_SOC_L);
    hw_model_set_readonly(&m->hw, BQ_REG_SOC_H);

    bq_model_update_outputs(m);
}

/* --- Helpers for tests --- */

__attribute__((unused))
static void bq_model_set_voltage(bq27441_model_t *m, uint16_t mv)
{
    m->voltage_mv = mv;
    bq_model_update_outputs(m);
}

__attribute__((unused))
static void bq_model_set_current(bq27441_model_t *m, int16_t ma)
{
    m->current_ma = ma;
    bq_model_update_outputs(m);
}

__attribute__((unused))
static void bq_model_set_soc(bq27441_model_t *m, uint16_t pct)
{
    m->soc_pct = pct;
    bq_model_update_outputs(m);
}

__attribute__((unused))
static void bq_model_set_temperature_01k(bq27441_model_t *m, uint16_t t01k)
{
    m->temp_01k = t01k;
    bq_model_update_outputs(m);
}

#endif /* HW_MODEL_BQ27441_H */
