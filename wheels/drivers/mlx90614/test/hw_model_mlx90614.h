// SPDX-License-Identifier: MIT
/*
 * hw_model_mlx90614.h — MLX90614 hardware register model
 *
 * Models Melexis MLX90614 SMBus behavior:
 * - RAM register 0x06: ambient temperature (read-only)
 * - RAM register 0x07: object 1 temperature (read-only)
 * - EEPROM register 0x24: emissivity (read/write)
 * - Temperature raw = (Kelvin * 50), so 25C = 298.15K = 14907.5 ~ 14908
 * - SMBus read word: S SA W cmd Sr SA R LSB MSB PEC P (Figure 6, p20)
 *
 * The model intercepts the I2C calls to simulate SMBus command-response
 * behavior over the register map.  A write call ends with STOP, so only
 * write_read, which keeps the repeated START, gets an answer.
 *
 * EEPROM: the emissivity cell needs an erase (write 0) first, p.13 and p.19, and
 * every write or erase keeps the EEPROM busy for 5 ms, p.16.  The manual does
 * not say what breaking those rules does, so this is the model's choice: a
 * write over an unerased cell comes out as the OR of old and new, and a write
 * while busy is lost.  Only the delay callback lets time pass.
 */

#ifndef HW_MODEL_MLX90614_H
#define HW_MODEL_MLX90614_H

#include "hw_model.h"

/* --- MLX90614 register addresses --- */
#define MLX_REG_TA         0x06u   /* ambient temperature */
#define MLX_REG_TOBJ1      0x07u   /* object 1 temperature */
#define MLX_REG_EMISSIVITY 0x24u   /* emissivity EEPROM */

/* p.16: "Wait at least 5ms (10ms to be on the safe side)" */
#define MLX_EEPROM_WRITE_MS 5u

/* Internal storage: 16-bit values stored as 2 consecutive byte slots
 * using (reg * 2) mapping to avoid collision in 256-byte space */
#define MLX_SLOT(reg) ((uint8_t)((reg) * 2))

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    float            true_object_temp_c;   /* simulated object temperature */
    float            true_ambient_temp_c;  /* simulated ambient temperature */
    uint8_t          last_cmd;             /* last command read */
    int              word_reads;           /* read words answered */
    uint32_t         eeprom_busy_ms;       /* time left of the last write */
    int              unerased_writes;      /* writes over an unerased cell */
    int              busy_writes;          /* writes lost while busy */
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
 * write():      a whole message: command byte + data for EEPROM writes, STOP
 * read():       a read after a STOP, which the sensor is not asked to answer
 * write_read(): read word: command byte, repeated START, LSB, MSB, PEC
 */

__attribute__((unused))
static bool mlx90614_model_write(void *ctx, uint8_t dev_addr,
                                 const uint8_t *data, uint16_t len)
{
    mlx90614_model_t *m = (mlx90614_model_t *)ctx;

    if (len == 0) return true;

    /* EEPROM write: cmd + LSB + MSB + PEC. p18: a NACK after the PEC means
     * the message had an error, so a bad frame changes nothing. */
    if (len >= 3 && data[0] == MLX_REG_EMISSIVITY) {
        uint8_t frame[4] = { (uint8_t)(dev_addr << 1), data[0], data[1], data[2] };
        if (len != 4 || mlx_model_pec(frame, 4) != data[3]) return false;

        if (m->eeprom_busy_ms != 0) {
            m->busy_writes++;
            return true;
        }

        uint8_t slot = MLX_SLOT(MLX_REG_EMISSIVITY);
        uint8_t lsb = data[1];
        uint8_t msb = data[2];
        bool erased  = (m->hw.regs[slot] | m->hw.regs[slot + 1]) == 0;
        bool erasing = (lsb | msb) == 0;
        if (!erased && !erasing) {
            m->unerased_writes++;
            lsb |= m->hw.regs[slot];
            msb |= m->hw.regs[slot + 1];
        }
        m->hw.regs[slot]     = lsb;
        m->hw.regs[slot + 1] = msb;
        m->eeprom_busy_ms = MLX_EEPROM_WRITE_MS;
    }

    return true;
}

/* p18: a read needs a repeated START, so the STOP before this call ended
 * the message and there is no command to answer.  The manual does not say
 * what the sensor does then; a model that refuses keeps the bug visible. */
__attribute__((unused))
static bool mlx90614_model_read(void *ctx, uint8_t dev_addr,
                                uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    memset(data, 0xFF, len);
    return false;
}

__attribute__((unused))
static bool mlx90614_model_write_read(void *ctx, uint8_t dev_addr,
                                      const uint8_t *tx, uint16_t tx_len,
                                      uint8_t *rx, uint16_t rx_len)
{
    mlx90614_model_t *m = (mlx90614_model_t *)ctx;

    if (tx_len != 1 || rx_len != 3) return false;

    /* Update temperatures before read */
    mlx90614_model_update_temps(m);

    uint8_t cmd = tx[0];
    uint8_t slot = MLX_SLOT(cmd);

    /* LSB, MSB, PEC */
    if (slot + 1 < HW_MODEL_REG_COUNT) {
        rx[0] = m->hw.regs[slot];      /* LSB */
        rx[1] = m->hw.regs[slot + 1];  /* MSB */
    } else {
        rx[0] = 0xFF;
        rx[1] = 0xFF;
    }
    uint8_t frame[5] = { (uint8_t)(dev_addr << 1), cmd,
                         (uint8_t)((dev_addr << 1) | 1u), rx[0], rx[1] };
    rx[2] = mlx_model_pec(frame, 5);

    m->last_cmd = cmd;
    m->word_reads++;
    return true;
}

/* Delay callback: the EEPROM finishes its write while the host waits */
__attribute__((unused))
static void mlx90614_model_delay(void *ctx, uint32_t ms)
{
    mlx90614_model_t *m = (mlx90614_model_t *)ctx;

    m->eeprom_busy_ms = (ms >= m->eeprom_busy_ms) ? 0 : m->eeprom_busy_ms - ms;
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
