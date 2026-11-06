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
 *
 * It refuses what the manual says the sensor refuses: commands during tPU,
 * during tSR and within 1 ms of the last command, and a read before the data
 * exists. Time passes only through sht30_model_delay().
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
/* Table 5: tPU maximum, the longest of the two tables */
#define SHT30_MODEL_TPU_US          1500u
/* Table 4: tSR maximum */
#define SHT30_MODEL_TSR_US          1500u
/* p.9: "a minimal waiting time of 1ms is needed before another command" */
#define SHT30_MODEL_CMD_GAP_US      1000u

/* --- Simulated environment --- */
typedef struct {
    float   true_temp_c;          /* simulated temperature */
    float   true_humidity_pct;    /* simulated humidity */
    /* Command state */
    uint8_t last_cmd_msb;
    uint8_t last_cmd_lsb;
    bool    measurement_pending;  /* single-shot command was sent */
    uint32_t measure_elapsed_ms;  /* delay time since that command */
    uint32_t busy_left_us;        /* no command gets through until this is 0 */
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

    /* p.12: "During the reset procedure the sensor will not process commands." */
    if (m->busy_left_us != 0)
        return false;

    m->last_cmd_msb = data[0];
    m->last_cmd_lsb = data[1];
    m->busy_left_us = SHT30_MODEL_CMD_GAP_US;

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
        m->measurement_pending = false;
        m->busy_left_us = SHT30_MODEL_TSR_US;
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

    /* Build response: temp_MSB, temp_LSB, temp_CRC, hum_MSB, hum_LSB, hum_CRC */
    uint16_t t_raw = sht30_model_temp_to_raw(m->true_temp_c);
    uint16_t h_raw = sht30_model_hum_to_raw(m->true_humidity_pct);
    uint8_t frame[6];

    frame[0] = (uint8_t)(t_raw >> 8);
    frame[1] = (uint8_t)(t_raw & 0xFF);
    frame[2] = sht30_model_crc8(&frame[0], 2);
    frame[3] = (uint8_t)(h_raw >> 8);
    frame[4] = (uint8_t)(h_raw & 0xFF);
    frame[5] = sht30_model_crc8(&frame[3], 2);

    /* p.10: the master "can abort the read transfer with a NACK condition after
     * any data byte". Past the frame nobody drives SDA. */
    for (uint16_t i = 0; i < len; i++)
        data[i] = (i < sizeof(frame)) ? frame[i] : 0xFF;

    m->measurement_pending = false;
    return true;
}

/* Delay callback: the only clock the model has */
__attribute__((unused))
static void sht30_model_delay(void *ctx, uint16_t ms)
{
    sht30_model_t *m = (sht30_model_t *)ctx;
    uint32_t us = (uint32_t)ms * 1000u;

    m->measure_elapsed_ms += ms;
    m->busy_left_us = (us < m->busy_left_us) ? m->busy_left_us - us : 0u;
}

static void sht30_model_init(sht30_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* p.9: the sensor needs tPU after power-up */
    m->busy_left_us = SHT30_MODEL_TPU_US;

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
