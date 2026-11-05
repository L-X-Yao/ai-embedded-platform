// SPDX-License-Identifier: MIT
/*
 * hw_model_scd40.h — SCD40 command-based hardware model
 *
 * The SCD40 is NOT register-based — it uses 16-bit commands over I2C:
 * - Write 2-byte command (optionally + 2-byte arg + CRC)
 * - Read N-word response (each word = 2 data bytes + 1 CRC byte)
 *
 * This model maps commands to responses with proper CRC-8.
 * Does NOT use mock_i2c.h register infrastructure (command-response only).
 */

#ifndef HW_MODEL_SCD40_H
#define HW_MODEL_SCD40_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SCD40 commands --- */
#define SCD40_CMD_START_PERIODIC_MSB  0x21u
#define SCD40_CMD_START_PERIODIC_LSB  0xB1u
#define SCD40_CMD_READ_MEASUREMENT_MSB 0xECu
#define SCD40_CMD_READ_MEASUREMENT_LSB 0x05u
#define SCD40_CMD_STOP_PERIODIC_MSB   0x3Fu
#define SCD40_CMD_STOP_PERIODIC_LSB   0x86u
#define SCD40_CMD_GET_SERIAL_MSB      0x36u
#define SCD40_CMD_GET_SERIAL_LSB      0x82u
#define SCD40_CMD_PERFORM_RESET_MSB   0xD3u
#define SCD40_CMD_PERFORM_RESET_LSB   0x04u
#define SCD40_CMD_SET_ALTITUDE_MSB    0x24u
#define SCD40_CMD_SET_ALTITUDE_LSB    0x27u
#define SCD40_CMD_GET_DATA_READY_MSB  0xE4u
#define SCD40_CMD_GET_DATA_READY_LSB  0xB8u

/* --- Simulated environment --- */
typedef struct {
    /* Physical state */
    uint16_t true_co2_ppm;
    float    true_temp_c;
    float    true_humidity_pct;
    uint16_t serial[3];
    uint16_t altitude_m;

    /* Command tracking */
    uint8_t  last_cmd_msb;
    uint8_t  last_cmd_lsb;
    bool     periodic_running;
    bool     data_ready;
    int      measure_count;
    int      reset_count;
    int      start_count;
    int      stop_count;

    /* Response state: which command was last written determines read */
    enum {
        SCD40_MODEL_RESP_NONE,
        SCD40_MODEL_RESP_MEASUREMENT,
        SCD40_MODEL_RESP_DATA_READY,
        SCD40_MODEL_RESP_SERIAL,
    } pending_response;
} scd40_model_t;

/* Sensirion CRC-8: poly 0x31, init 0xFF */
static uint8_t scd40_model_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80u)
                crc = (uint8_t)((uint8_t)(crc << 1) ^ 0x31u);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/** Encode one word (2 data bytes + CRC) into buf[0..2]. */
static void scd40_model_encode_word(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
    buf[2] = scd40_model_crc8(buf, 2);
}

/* Convert temperature C to SCD40 raw: raw = (T + 45) / 175 * 65535 */
static uint16_t scd40_model_temp_to_raw(float temp_c)
{
    float norm = (temp_c + 45.0f) / 175.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65535.0f + 0.5f);
}

/* Convert humidity % to SCD40 raw: raw = H / 100 * 65535 */
static uint16_t scd40_model_hum_to_raw(float humidity_pct)
{
    float norm = humidity_pct / 100.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65535.0f + 0.5f);
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool scd40_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    scd40_model_t *m = (scd40_model_t *)ctx;

    if (len < 2) return true;

    m->last_cmd_msb = data[0];
    m->last_cmd_lsb = data[1];
    m->pending_response = SCD40_MODEL_RESP_NONE;

    /* start_periodic */
    if (data[0] == SCD40_CMD_START_PERIODIC_MSB &&
        data[1] == SCD40_CMD_START_PERIODIC_LSB) {
        m->periodic_running = true;
        m->start_count++;
    }
    /* stop_periodic */
    else if (data[0] == SCD40_CMD_STOP_PERIODIC_MSB &&
             data[1] == SCD40_CMD_STOP_PERIODIC_LSB) {
        m->periodic_running = false;
        m->stop_count++;
    }
    /* read_measurement */
    else if (data[0] == SCD40_CMD_READ_MEASUREMENT_MSB &&
             data[1] == SCD40_CMD_READ_MEASUREMENT_LSB) {
        m->pending_response = SCD40_MODEL_RESP_MEASUREMENT;
        m->measure_count++;
    }
    /* get_data_ready_status */
    else if (data[0] == SCD40_CMD_GET_DATA_READY_MSB &&
             data[1] == SCD40_CMD_GET_DATA_READY_LSB) {
        m->pending_response = SCD40_MODEL_RESP_DATA_READY;
    }
    /* get_serial */
    else if (data[0] == SCD40_CMD_GET_SERIAL_MSB &&
             data[1] == SCD40_CMD_GET_SERIAL_LSB) {
        m->pending_response = SCD40_MODEL_RESP_SERIAL;
    }
    /* perform_reset */
    else if (data[0] == SCD40_CMD_PERFORM_RESET_MSB &&
             data[1] == SCD40_CMD_PERFORM_RESET_LSB) {
        m->periodic_running = false;
        m->reset_count++;
    }
    /* set_altitude (5 bytes: cmd + arg + crc) */
    else if (data[0] == SCD40_CMD_SET_ALTITUDE_MSB &&
             data[1] == SCD40_CMD_SET_ALTITUDE_LSB && len >= 5) {
        m->altitude_m = (uint16_t)((uint16_t)data[2] << 8 | data[3]);
    }

    return true;
}

__attribute__((unused))
static bool scd40_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    scd40_model_t *m = (scd40_model_t *)ctx;

    switch (m->pending_response) {
    case SCD40_MODEL_RESP_NONE:
        memset(data, 0xFF, len);
        break;
    case SCD40_MODEL_RESP_MEASUREMENT: {
        if (len < 9) { memset(data, 0xFF, len); return true; }
        uint16_t t_raw = scd40_model_temp_to_raw(m->true_temp_c);
        uint16_t h_raw = scd40_model_hum_to_raw(m->true_humidity_pct);
        scd40_model_encode_word(&data[0], m->true_co2_ppm);
        scd40_model_encode_word(&data[3], t_raw);
        scd40_model_encode_word(&data[6], h_raw);
        break;
    }
    case SCD40_MODEL_RESP_DATA_READY: {
        if (len < 3) { memset(data, 0xFF, len); return true; }
        /* Table 23: low 11 bits 0 = not ready, example word 0x8000 */
        uint16_t status = m->data_ready ? 0x8006u : 0x8000u;
        scd40_model_encode_word(&data[0], status);
        break;
    }
    case SCD40_MODEL_RESP_SERIAL: {
        if (len < 9) { memset(data, 0xFF, len); return true; }
        scd40_model_encode_word(&data[0], m->serial[0]);
        scd40_model_encode_word(&data[3], m->serial[1]);
        scd40_model_encode_word(&data[6], m->serial[2]);
        break;
    }
    default:
        memset(data, 0xFF, len);
        break;
    }

    return true;
}

/* Delay callback (no-op for model) */
__attribute__((unused))
static void scd40_model_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static void scd40_model_init(scd40_model_t *m)
{
    memset(m, 0, sizeof(*m));
    /* Default environment: 25C, 50% RH, 415 ppm CO2 */
    m->true_co2_ppm      = 415;
    m->true_temp_c       = 25.0f;
    m->true_humidity_pct = 50.0f;
    m->serial[0]         = 0xBEEF;
    m->serial[1]         = 0xCAFE;
    m->serial[2]         = 0xDEAD;
    m->data_ready        = true;
}

#endif /* HW_MODEL_SCD40_H */
