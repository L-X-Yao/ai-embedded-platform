// SPDX-License-Identifier: MIT
/*
 * hw_model_sht30.h — SHT30 command-based hardware model
 *
 * The SHT30 is NOT register-based — it uses 16-bit commands over I2C:
 * - Write 2-byte command (MSB, LSB)
 * - Read 6-byte response (temp_MSB, temp_LSB, temp_CRC, hum_MSB, hum_LSB, hum_CRC)
 *
 * This model maps commands to responses with proper CRC-8.
 * Does NOT use hw_model.h register infrastructure (command-response only).
 */

#ifndef HW_MODEL_SHT30_H
#define HW_MODEL_SHT30_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SHT30 commands (from datasheet) --- */
#define SHT30_CMD_SINGLE_HIGH_MSB   0x24u
#define SHT30_CMD_SINGLE_HIGH_LSB   0x00u
#define SHT30_CMD_SINGLE_MED_MSB    0x24u
#define SHT30_CMD_SINGLE_MED_LSB    0x0Bu
#define SHT30_CMD_SINGLE_LOW_MSB    0x24u
#define SHT30_CMD_SINGLE_LOW_LSB    0x16u
#define SHT30_CMD_SOFT_RESET_MSB    0x30u
#define SHT30_CMD_SOFT_RESET_LSB    0xA2u
#define SHT30_CMD_HEATER_ON_MSB     0x30u
#define SHT30_CMD_HEATER_ON_LSB     0x6Du
#define SHT30_CMD_HEATER_OFF_MSB    0x30u
#define SHT30_CMD_HEATER_OFF_LSB    0x66u

/* Table 4: tMEAS,h maximum */
#define SHT30_MODEL_TMEAS_H_MS      15u

/* --- Simulated environment --- */
typedef struct {
    float   true_temp_c;          /* simulated temperature */
    float   true_humidity_pct;    /* simulated humidity */
    /* Command state */
    uint8_t last_cmd_msb;
    uint8_t last_cmd_lsb;
    bool    measurement_pending;  /* single-shot command was sent */
    uint32_t measure_elapsed_ms;  /* delay time since that command */
    bool    reset_pending;
    int     measure_count;
    int     reset_count;
} sht30_model_t;

/* Sensirion CRC-8: poly 0x31, init 0xFF */
static uint8_t sht30_model_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Convert temperature C to SHT30 raw: raw = (T + 45) / 175 * 65535 */
static uint16_t sht30_model_temp_to_raw(float temp_c)
{
    float norm = (temp_c + 45.0f) / 175.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65535.0f + 0.5f);
}

/* Convert humidity % to SHT30 raw: raw = H / 100 * 65535 */
static uint16_t sht30_model_hum_to_raw(float humidity_pct)
{
    float norm = humidity_pct / 100.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65535.0f + 0.5f);
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool sht30_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    sht30_model_t *m = (sht30_model_t *)ctx;

    if (len < 2) return true;

    m->last_cmd_msb = data[0];
    m->last_cmd_lsb = data[1];

    /* Check if this is a measurement command */
    if (data[0] == SHT30_CMD_SINGLE_HIGH_MSB &&
        (data[1] == SHT30_CMD_SINGLE_HIGH_LSB ||
         data[1] == SHT30_CMD_SINGLE_MED_LSB ||
         data[1] == SHT30_CMD_SINGLE_LOW_LSB)) {
        m->measurement_pending = true;
        m->measure_elapsed_ms = 0;
        m->measure_count++;
    }

    /* Check for soft reset */
    if (data[0] == SHT30_CMD_SOFT_RESET_MSB &&
        data[1] == SHT30_CMD_SOFT_RESET_LSB) {
        m->reset_pending = true;
        m->reset_count++;
    }

    return true;
}

__attribute__((unused))
static bool sht30_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    sht30_model_t *m = (sht30_model_t *)ctx;

    /* p.10, no clock stretching: "the sensor responds to a read header with a
     * not acknowledge (NACK), if no data is present." */
    if (!m->measurement_pending ||
        m->measure_elapsed_ms < SHT30_MODEL_TMEAS_H_MS)
        return false;

    if (len < 6) {
        memset(data, 0xFF, len);
        return true;
    }

    /* Build response: temp_MSB, temp_LSB, temp_CRC, hum_MSB, hum_LSB, hum_CRC */
    uint16_t t_raw = sht30_model_temp_to_raw(m->true_temp_c);
    uint16_t h_raw = sht30_model_hum_to_raw(m->true_humidity_pct);

    data[0] = (uint8_t)(t_raw >> 8);
    data[1] = (uint8_t)(t_raw & 0xFF);
    data[2] = sht30_model_crc8(&data[0], 2);
    data[3] = (uint8_t)(h_raw >> 8);
    data[4] = (uint8_t)(h_raw & 0xFF);
    data[5] = sht30_model_crc8(&data[3], 2);

    m->measurement_pending = false;
    return true;
}

/* Delay callback: the only clock the model has */
__attribute__((unused))
static void sht30_model_delay(void *ctx, uint16_t ms)
{
    sht30_model_t *m = (sht30_model_t *)ctx;
    m->measure_elapsed_ms += ms;
}

static void sht30_model_init(sht30_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Default environment: 25C, 50% RH */
    m->true_temp_c = 25.0f;
    m->true_humidity_pct = 50.0f;
}

/* Helper: set simulated temperature */
static void sht30_model_set_temperature(sht30_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

/* Helper: set simulated humidity */
static void sht30_model_set_humidity(sht30_model_t *m, float humidity_pct)
{
    m->true_humidity_pct = humidity_pct;
}

#endif /* HW_MODEL_SHT30_H */
