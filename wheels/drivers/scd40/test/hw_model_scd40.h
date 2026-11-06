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
 *
 * Time only passes through scd40_model_delay, which is also how a measurement
 * makes progress. As in the manual:
 * - a command or a read header inside the execution time of the last command
 *   gets a NACK (Table 9, p.7, p.10)
 * - commands marked "no" under "During meas." get a NACK while a periodic
 *   measurement runs, and are counted in forbidden_cmds (Table 9, p.9)
 * - a sample arrives every 5 s, read_measurement empties the buffer, and an
 *   empty buffer gets a NACK (p.9)
 * - a write sequence without a good checksum is refused (p.7)
 * - reinit reloads the altitude saved in EEPROM, which is 0 here (p.17)
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
#define SCD40_CMD_REINIT_MSB          0x36u
#define SCD40_CMD_REINIT_LSB          0x46u
#define SCD40_CMD_SET_ALTITUDE_MSB    0x24u
#define SCD40_CMD_SET_ALTITUDE_LSB    0x27u
#define SCD40_CMD_GET_DATA_READY_MSB  0xE4u
#define SCD40_CMD_GET_DATA_READY_LSB  0xB8u

/* p.9: "The signal update interval is 5 seconds." */
#define SCD40_MODEL_INTERVAL_MS       5000u

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
    int      forbidden_cmds;

    /* Time, in ms */
    uint32_t now_ms;
    uint32_t busy_until_ms;
    uint32_t next_sample_ms;

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

/* --- Commands, Table 9 on p.8 --- */

typedef struct {
    uint8_t  msb;
    uint8_t  lsb;
    uint16_t exec_ms;
    bool     during_meas;
} scd40_model_cmd_t;

static const scd40_model_cmd_t *scd40_model_find_cmd(uint8_t msb, uint8_t lsb)
{
    static const scd40_model_cmd_t cmds[] = {
        { SCD40_CMD_START_PERIODIC_MSB,   SCD40_CMD_START_PERIODIC_LSB,     0, false },
        { SCD40_CMD_READ_MEASUREMENT_MSB, SCD40_CMD_READ_MEASUREMENT_LSB,   1, true  },
        { SCD40_CMD_STOP_PERIODIC_MSB,    SCD40_CMD_STOP_PERIODIC_LSB,    500, true  },
        { SCD40_CMD_SET_ALTITUDE_MSB,     SCD40_CMD_SET_ALTITUDE_LSB,       1, false },
        { SCD40_CMD_GET_DATA_READY_MSB,   SCD40_CMD_GET_DATA_READY_LSB,     1, true  },
        { SCD40_CMD_GET_SERIAL_MSB,       SCD40_CMD_GET_SERIAL_LSB,         1, false },
        { SCD40_CMD_REINIT_MSB,           SCD40_CMD_REINIT_LSB,            30, false },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        if (cmds[i].msb == msb && cmds[i].lsb == lsb) {
            return &cmds[i];
        }
    }
    return NULL;
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool scd40_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    scd40_model_t *m = (scd40_model_t *)ctx;

    if (len < 2) return true;

    /* p.7: "Commands must not be sent while a preceding command is being
     * processed." p.10: after stop the sensor "will only respond to other
     * commands 500 ms after" it */
    if (m->now_ms < m->busy_until_ms) return false;

    m->last_cmd_msb = data[0];
    m->last_cmd_lsb = data[1];
    m->pending_response = SCD40_MODEL_RESP_NONE;

    const scd40_model_cmd_t *cmd = scd40_model_find_cmd(data[0], data[1]);
    if (!cmd) return true;   /* not an SCD4x command: nothing happens */

    /* p.9: "no other commands must be issued" during a periodic measurement */
    if (m->periodic_running && !cmd->during_meas) {
        m->forbidden_cmds++;
        return false;
    }

    /* start_periodic */
    if (data[0] == SCD40_CMD_START_PERIODIC_MSB &&
        data[1] == SCD40_CMD_START_PERIODIC_LSB) {
        m->periodic_running = true;
        m->data_ready = false;
        m->next_sample_ms = m->now_ms + SCD40_MODEL_INTERVAL_MS;
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
    /* reinit: p.17, "reloading user settings from EEPROM". The model has no
     * persist_settings, so that is the factory altitude of 0 */
    else if (data[0] == SCD40_CMD_REINIT_MSB &&
             data[1] == SCD40_CMD_REINIT_LSB) {
        m->altitude_m = 0;
        m->reset_count++;
    }
    /* set_altitude. p.7: "In write direction it is mandatory to transmit the
     * checksum." */
    else {
        if (len != 5 || scd40_model_crc8(&data[2], 2) != data[4]) return false;
        m->altitude_m = (uint16_t)((uint16_t)data[2] << 8 | data[3]);
    }

    m->busy_until_ms = m->now_ms + cmd->exec_ms;
    return true;
}

__attribute__((unused))
static bool scd40_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    scd40_model_t *m = (scd40_model_t *)ctx;
    uint8_t  resp[9];
    uint16_t resp_len;

    /* p.7: "the sensor needs the execution time (see Table 9) to respond to the
     * I2C read header with an ACK bit" */
    if (m->now_ms < m->busy_until_ms) return false;

    switch (m->pending_response) {
    case SCD40_MODEL_RESP_MEASUREMENT: {
        /* p.9: "If no data is available in the buffer, the sensor returns a
         * NACK." */
        if (!m->data_ready) return false;
        scd40_model_encode_word(&resp[0], m->true_co2_ppm);
        scd40_model_encode_word(&resp[3], scd40_model_temp_to_raw(m->true_temp_c));
        scd40_model_encode_word(&resp[6], scd40_model_hum_to_raw(m->true_humidity_pct));
        resp_len = 9;
        /* p.9: "the buffer is emptied upon read-out" */
        m->data_ready = false;
        break;
    }
    case SCD40_MODEL_RESP_DATA_READY:
        /* Table 23: low 11 bits 0 = not ready, example word 0x8000 */
        scd40_model_encode_word(&resp[0], m->data_ready ? 0x8006u : 0x8000u);
        resp_len = 3;
        break;
    case SCD40_MODEL_RESP_SERIAL:
        scd40_model_encode_word(&resp[0], m->serial[0]);
        scd40_model_encode_word(&resp[3], m->serial[1]);
        scd40_model_encode_word(&resp[6], m->serial[2]);
        resp_len = 9;
        break;
    default:
        return false;   /* no command waits for a read */
    }

    /* p.9: the master can end the read "after any data byte". Past the end of
     * the response the bus stays high. */
    memset(data, 0xFF, len);
    memcpy(data, resp, len < resp_len ? len : resp_len);
    m->pending_response = SCD40_MODEL_RESP_NONE;
    return true;
}

/* Delay callback: the only way time passes */
__attribute__((unused))
static void scd40_model_delay(void *ctx, uint32_t ms)
{
    scd40_model_t *m = (scd40_model_t *)ctx;

    m->now_ms += ms;
    while (m->periodic_running && m->now_ms >= m->next_sample_ms) {
        m->data_ready = true;
        m->next_sample_ms += SCD40_MODEL_INTERVAL_MS;
    }
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
}

#endif /* HW_MODEL_SCD40_H */
