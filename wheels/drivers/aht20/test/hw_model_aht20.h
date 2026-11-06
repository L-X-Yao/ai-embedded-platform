// SPDX-License-Identifier: MIT
/*
 * hw_model_aht20.h -- AHT20 command-based hardware model
 *
 * The AHT20 is NOT register-based -- it uses command-response over I2C:
 * - Write 3-byte init command: [0xBE, 0x08, 0x00]
 * - Write 3-byte trigger:      [0xAC, 0x33, 0x00]
 * - Write 1-byte soft reset:   [0xBA]
 * - Read the frame: [status, hum[19:12], hum[11:4],
 *     hum[3:0]|temp[19:16], temp[15:8], temp[7:0], CRC-8]
 *   Status is its first byte (bit 7 = busy, bit 3 = calibrated), and the master
 *   stops after as many bytes as it wants.
 *
 * CRC-8: poly 0x31, init 0xFF (same as Sensirion).
 * Fixed I2C address: 0x38.
 * Datasheet range: temperature -40..+85 C, humidity 0..100 %RH.
 *
 * Follows AHT20 Product manuals V1.1. The sensor gives no ACK until 40 ms after
 * power-on and during a soft reset, bit 3 sets 10 ms after the init command,
 * and bit 7 stays set until the measurement ends. While it is set the frame
 * still holds the last result with a good CRC, so only bit 7 tells them apart.
 * Time passes only through aht20_model_delay().
 *
 * Does NOT use hw_model.h register infrastructure (command-response only).
 */

#ifndef HW_MODEL_AHT20_H
#define HW_MODEL_AHT20_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- AHT20 commands (from datasheet) --- */
#define AHT20_CMD_INIT_0        0xBEu
#define AHT20_CMD_INIT_1        0x08u
#define AHT20_CMD_INIT_2        0x00u
#define AHT20_CMD_TRIGGER_0     0xACu
#define AHT20_CMD_TRIGGER_1     0x33u
#define AHT20_CMD_TRIGGER_2     0x00u
#define AHT20_CMD_SOFT_RESET    0xBAu

/* --- Status bits --- */
#define AHT20_STATUS_BUSY       (1u << 7)
#define AHT20_STATUS_CAL        (1u << 3)

#define AHT20_MODEL_ADDR        0x38u

/* --- Timing: p.8 steps 1 and 3, and p.9 --- */
#define AHT20_MODEL_POWER_ON_MS 40u
#define AHT20_MODEL_INIT_MS     10u
#define AHT20_MODEL_MEASURE_MS  80u
#define AHT20_MODEL_RESET_MS    20u

/* p.9 divides by 2^20, and the raw values are 20 bits wide */
#define AHT20_MODEL_RAW_FULL    1048576.0f
#define AHT20_MODEL_RAW_MAX     0xFFFFFu

/* --- Simulated environment --- */
typedef struct {
    float   true_temp_c;           /* simulated temperature (-40..+85) */
    float   true_humidity_pct;     /* simulated humidity (0..100)      */
    bool    calibrated;            /* calibration status bit           */
    bool    calibration_fails;     /* the init command leaves bit 3 at 0 */
    uint32_t measure_need_ms;      /* how long a measurement takes     */
    /* Command state */
    uint8_t last_cmd;
    uint32_t power_on_left_ms;
    uint32_t reset_left_ms;
    uint32_t init_left_ms;
    uint32_t measure_left_ms;
    uint32_t last_h_raw;           /* result of the last measurement   */
    uint32_t last_t_raw;
    int     measure_count;
    int     reset_count;
    int     init_count;
} aht20_model_t;

/* CRC-8: poly 0x31, init 0xFF (identical to Sensirion / AHT20 datasheet) */
static uint8_t aht20_model_crc8(const uint8_t *data, uint16_t len)
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

/* A fraction of the range to 20 bits, which stop one step short of 2^20 */
static uint32_t aht20_model_norm_to_raw(float norm)
{
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    uint32_t raw = (uint32_t)(norm * AHT20_MODEL_RAW_FULL + 0.5f);
    return (raw > AHT20_MODEL_RAW_MAX) ? AHT20_MODEL_RAW_MAX : raw;
}

/* Convert temperature C to AHT20 20-bit raw: raw = (T + 50) / 200 * 2^20 */
static uint32_t aht20_model_temp_to_raw(float temp_c)
{
    return aht20_model_norm_to_raw((temp_c + 50.0f) / 200.0f);
}

/* Convert humidity % to AHT20 20-bit raw: raw = H / 100 * 2^20 */
static uint32_t aht20_model_hum_to_raw(float humidity_pct)
{
    return aht20_model_norm_to_raw(humidity_pct / 100.0f);
}

static bool aht20_model_ready(const aht20_model_t *m)
{
    return m->power_on_left_ms == 0 && m->reset_left_ms == 0;
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool aht20_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    aht20_model_t *m = (aht20_model_t *)ctx;

    if (dev_addr != AHT20_MODEL_ADDR || !aht20_model_ready(m))
        return false;

    if (len == 0) return true;

    m->last_cmd = data[0];

    /* 3-byte init command: [0xBE, 0x08, 0x00] */
    if (len == 3 &&
        data[0] == AHT20_CMD_INIT_0 &&
        data[1] == AHT20_CMD_INIT_1 &&
        data[2] == AHT20_CMD_INIT_2) {
        m->init_left_ms = AHT20_MODEL_INIT_MS;
        m->init_count++;
        return true;
    }

    /* 3-byte trigger command: [0xAC, 0x33, 0x00] */
    if (len == 3 &&
        data[0] == AHT20_CMD_TRIGGER_0 &&
        data[1] == AHT20_CMD_TRIGGER_1 &&
        data[2] == AHT20_CMD_TRIGGER_2) {
        m->measure_left_ms = m->measure_need_ms;
        m->measure_count++;
        return true;
    }

    /* 1-byte soft reset: [0xBA] */
    if (len == 1 && data[0] == AHT20_CMD_SOFT_RESET) {
        m->reset_count++;
        m->reset_left_ms = AHT20_MODEL_RESET_MS;
        m->measure_left_ms = 0;
        m->init_left_ms = 0;
        return true;
    }

    return true;
}

__attribute__((unused))
static bool aht20_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    aht20_model_t *m = (aht20_model_t *)ctx;

    if (dev_addr != AHT20_MODEL_ADDR || !aht20_model_ready(m))
        return false;

    /* [status, hum[19:12], hum[11:4], hum[3:0]|temp[19:16],
     *  temp[15:8], temp[7:0], CRC-8] */
    uint8_t frame[7];
    uint32_t h_raw = m->last_h_raw;
    uint32_t t_raw = m->last_t_raw;

    frame[0] = 0x00;
    if (m->calibrated)
        frame[0] |= AHT20_STATUS_CAL;
    if (m->measure_left_ms != 0 || m->init_left_ms != 0)
        frame[0] |= AHT20_STATUS_BUSY;
    frame[1] = (uint8_t)((h_raw >> 12) & 0xFF);
    frame[2] = (uint8_t)((h_raw >> 4) & 0xFF);
    frame[3] = (uint8_t)(((h_raw & 0x0F) << 4) | ((t_raw >> 16) & 0x0F));
    frame[4] = (uint8_t)((t_raw >> 8) & 0xFF);
    frame[5] = (uint8_t)(t_raw & 0xFF);
    frame[6] = aht20_model_crc8(frame, 6);

    /* p.8: the CRC byte is read "as needed". Past the frame nobody drives SDA. */
    for (uint16_t i = 0; i < len; i++)
        data[i] = (i < sizeof(frame)) ? frame[i] : 0xFF;

    return true;
}

static uint32_t aht20_model_less(uint32_t left_ms, uint16_t ms)
{
    return (ms < left_ms) ? left_ms - ms : 0u;
}

/* Delay callback: the only clock the model has */
__attribute__((unused))
static void aht20_model_delay(void *ctx, uint16_t ms)
{
    aht20_model_t *m = (aht20_model_t *)ctx;

    m->power_on_left_ms = aht20_model_less(m->power_on_left_ms, ms);
    m->reset_left_ms = aht20_model_less(m->reset_left_ms, ms);

    if (m->init_left_ms != 0) {
        m->init_left_ms = aht20_model_less(m->init_left_ms, ms);
        if (m->init_left_ms == 0 && !m->calibration_fails)
            m->calibrated = true;
    }

    if (m->measure_left_ms != 0) {
        m->measure_left_ms = aht20_model_less(m->measure_left_ms, ms);
        if (m->measure_left_ms == 0) {
            m->last_h_raw = aht20_model_hum_to_raw(m->true_humidity_pct);
            m->last_t_raw = aht20_model_temp_to_raw(m->true_temp_c);
        }
    }
}

static void aht20_model_init(aht20_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Default environment: 25C, 50% RH, calibrated */
    m->true_temp_c = 25.0f;
    m->true_humidity_pct = 50.0f;
    m->calibrated = true;
    m->measure_need_ms = AHT20_MODEL_MEASURE_MS;
    m->power_on_left_ms = AHT20_MODEL_POWER_ON_MS;
}

/* Helper: set simulated temperature */
__attribute__((unused))
static void aht20_model_set_temperature(aht20_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

/* Helper: set simulated humidity */
__attribute__((unused))
static void aht20_model_set_humidity(aht20_model_t *m, float humidity_pct)
{
    m->true_humidity_pct = humidity_pct;
}

/* Helper: set calibration state (bit 3 in status byte) */
__attribute__((unused))
static void aht20_model_set_calibrated(aht20_model_t *m, bool calibrated)
{
    m->calibrated = calibrated;
}

/* Helper: make the init command leave bit 3 at 0 */
__attribute__((unused))
static void aht20_model_set_calibration_fails(aht20_model_t *m, bool fails)
{
    m->calibration_fails = fails;
}

/* Helper: a measurement past the 80 ms, p.8: "otherwise, continue to wait." */
__attribute__((unused))
static void aht20_model_set_measure_ms(aht20_model_t *m, uint32_t ms)
{
    m->measure_need_ms = ms;
}

#endif /* HW_MODEL_AHT20_H */
