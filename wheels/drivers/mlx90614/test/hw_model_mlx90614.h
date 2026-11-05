// SPDX-License-Identifier: MIT
/*
 * hw_model_mlx90614.h — MLX90614 hardware register model
 *
 * Models Melexis MLX90614 SMBus behavior:
 * - RAM register 0x06: ambient temperature (read-only)
 * - RAM register 0x07: object 1 temperature (read-only)
 * - EEPROM register 0x24: emissivity (read/write)
 * - Temperature raw = (Kelvin * 50), so 25C = 298.15K = 14907.5 ~ 14908
 * - SMBus protocol: write cmd byte, read 3 bytes (LSB, MSB, PEC)
 *
 * The model intercepts raw I2C write/read calls to simulate SMBus
 * command-response behavior over the register map.
 */

#ifndef HW_MODEL_MLX90614_H
#define HW_MODEL_MLX90614_H

#include "hw_model.h"

/* --- MLX90614 register addresses --- */
#define MLX_REG_TA         0x06u   /* ambient temperature */
#define MLX_REG_TOBJ1      0x07u   /* object 1 temperature */
#define MLX_REG_EMISSIVITY 0x24u   /* emissivity EEPROM */

/* Internal storage: 16-bit values stored as 2 consecutive byte slots
 * using (reg * 2) mapping to avoid collision in 256-byte space */
#define MLX_SLOT(reg) ((uint8_t)((reg) * 2))

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    float            true_object_temp_c;   /* simulated object temperature */
    float            true_ambient_temp_c;  /* simulated ambient temperature */
    uint8_t          last_cmd;             /* last command byte written */
    bool             cmd_pending;          /* a command was written */
} mlx90614_model_t;

/* Convert Celsius to MLX90614 raw (raw = K * 50 = (C + 273.15) * 50) */
static uint16_t mlx_temp_to_raw(float temp_c)
{
    float kelvin = temp_c + 273.15f;
    return (uint16_t)(kelvin * 50.0f + 0.5f);
}

/* Update temperature registers from true values */
static void mlx90614_model_update_temps(mlx90614_model_t *m)
{
    uint16_t ta = mlx_temp_to_raw(m->true_ambient_temp_c);
    m->hw.regs[MLX_SLOT(MLX_REG_TA)]     = (uint8_t)(ta & 0xFF);
    m->hw.regs[MLX_SLOT(MLX_REG_TA) + 1] = (uint8_t)(ta >> 8);

    uint16_t tobj = mlx_temp_to_raw(m->true_object_temp_c);
    m->hw.regs[MLX_SLOT(MLX_REG_TOBJ1)]     = (uint8_t)(tobj & 0xFF);
    m->hw.regs[MLX_SLOT(MLX_REG_TOBJ1) + 1] = (uint8_t)(tobj >> 8);
}

/* PEC, p18: CRC-8 with polynomial X8+X2+X1+1 over every byte on the wire */
static uint8_t mlx_model_pec(const uint8_t *frame, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= frame[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            bool top = (crc & 0x80u) != 0u;
            crc = (uint8_t)(crc << 1);
            if (top) crc ^= 0x07u;
        }
    }
    return crc;
}

/*
 * SMBus I2C callbacks for MLX90614.
 * write(): receives command byte (+ optional data for EEPROM writes)
 * read():  returns response for last command (LSB, MSB, PEC)
 */

__attribute__((unused))
static bool mlx90614_model_write(void *ctx, uint8_t dev_addr,
                                 const uint8_t *data, uint16_t len)
{
    mlx90614_model_t *m = (mlx90614_model_t *)ctx;

    if (len == 0) return true;

    m->last_cmd = data[0];
    m->cmd_pending = true;

    /* EEPROM write: cmd + LSB + MSB + PEC. p18: a NACK after the PEC means
     * the message had an error, so a bad frame changes nothing. */
    if (len >= 3 && data[0] == MLX_REG_EMISSIVITY) {
        uint8_t frame[4] = { (uint8_t)(dev_addr << 1), data[0], data[1], data[2] };
        if (len != 4 || mlx_model_pec(frame, 4) != data[3]) return false;

        uint8_t slot = MLX_SLOT(MLX_REG_EMISSIVITY);
        m->hw.regs[slot]     = data[1];  /* LSB */
        m->hw.regs[slot + 1] = data[2];  /* MSB */
    }

    return true;
}

__attribute__((unused))
static bool mlx90614_model_read(void *ctx, uint8_t dev_addr,
                                uint8_t *data, uint16_t len)
{
    mlx90614_model_t *m = (mlx90614_model_t *)ctx;

    if (!m->cmd_pending || len < 3) {
        memset(data, 0xFF, len);
        return true;
    }

    /* Update temperatures before read */
    mlx90614_model_update_temps(m);

    uint8_t cmd = m->last_cmd;
    uint8_t slot = MLX_SLOT(cmd);

    /* LSB, MSB, PEC */
    if (slot + 1 < HW_MODEL_REG_COUNT) {
        data[0] = m->hw.regs[slot];      /* LSB */
        data[1] = m->hw.regs[slot + 1];  /* MSB */
    } else {
        data[0] = 0xFF;
        data[1] = 0xFF;
    }
    uint8_t frame[5] = { (uint8_t)(dev_addr << 1), cmd,
                         (uint8_t)((dev_addr << 1) | 1u), data[0], data[1] };
    data[2] = mlx_model_pec(frame, 5);

    m->cmd_pending = false;
    return true;
}

/* Delay callback (no-op: the model has no EEPROM busy time) */
__attribute__((unused))
static void mlx90614_model_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static void mlx90614_model_init(mlx90614_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Temperature registers are read-only (set by model) */
    hw_model_set_readonly(&m->hw, MLX_SLOT(MLX_REG_TA));
    hw_model_set_readonly(&m->hw, MLX_SLOT(MLX_REG_TA) + 1);
    hw_model_set_readonly(&m->hw, MLX_SLOT(MLX_REG_TOBJ1));
    hw_model_set_readonly(&m->hw, MLX_SLOT(MLX_REG_TOBJ1) + 1);

    /* Emissivity default: 1.0 = 0xFFFF */
    uint8_t em_slot = MLX_SLOT(MLX_REG_EMISSIVITY);
    m->hw.reset_values[em_slot]     = 0xFF;
    m->hw.reset_values[em_slot + 1] = 0xFF;

    /* Default environment: ~25C object, ~22C ambient */
    m->true_object_temp_c  = 25.0f;
    m->true_ambient_temp_c = 22.0f;

    hw_model_reset(&m->hw);
    mlx90614_model_update_temps(m);
}

/* Helper: set simulated object temperature */
static void mlx90614_model_set_object_temp(mlx90614_model_t *m, float temp_c)
{
    m->true_object_temp_c = temp_c;
    mlx90614_model_update_temps(m);
}

/* Helper: set simulated ambient temperature */
static void mlx90614_model_set_ambient_temp(mlx90614_model_t *m, float temp_c)
{
    m->true_ambient_temp_c = temp_c;
    mlx90614_model_update_temps(m);
}

#endif /* HW_MODEL_MLX90614_H */
