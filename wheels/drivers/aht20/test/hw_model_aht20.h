/*
 * hw_model_aht20.h -- AHT20 command-based hardware model
 *
 * The AHT20 is NOT register-based -- it uses command-response over I2C:
 * - Write 3-byte init command: [0xBE, 0x08, 0x00]
 * - Write 3-byte trigger:      [0xAC, 0x33, 0x00]
 * - Write 1-byte soft reset:   [0xBA]
 * - Read 1-byte status (bit 7 = busy, bit 3 = calibrated)
 * - Read 7-byte measurement: [status, hum[19:12], hum[11:4],
 *     hum[3:0]|temp[19:16], temp[15:8], temp[7:0], CRC-8]
 *
 * CRC-8: poly 0x31, init 0xFF (same as Sensirion).
 * Fixed I2C address: 0x38.
 * Datasheet range: temperature -40..+85 C, humidity 0..100 %RH.
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

/* --- Simulated environment --- */
typedef struct {
    float   true_temp_c;           /* simulated temperature (-40..+85) */
    float   true_humidity_pct;     /* simulated humidity (0..100)      */
    bool    calibrated;            /* calibration status bit           */
    /* Command state */
    uint8_t last_cmd;
    bool    measurement_pending;
    bool    init_sent;
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

/* Convert temperature C to AHT20 20-bit raw: raw = (T + 50) / 200 * 2^20 */
static uint32_t aht20_model_temp_to_raw(float temp_c)
{
    float norm = (temp_c + 50.0f) / 200.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint32_t)(norm * 1048576.0f + 0.5f);
}

/* Convert humidity % to AHT20 20-bit raw: raw = H / 100 * 2^20 */
static uint32_t aht20_model_hum_to_raw(float humidity_pct)
{
    float norm = humidity_pct / 100.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint32_t)(norm * 1048576.0f + 0.5f);
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool aht20_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    aht20_model_t *m = (aht20_model_t *)ctx;

    if (len == 0) return true;

    m->last_cmd = data[0];

    /* 3-byte init command: [0xBE, 0x08, 0x00] */
    if (len >= 3 &&
        data[0] == AHT20_CMD_INIT_0 &&
        data[1] == AHT20_CMD_INIT_1 &&
        data[2] == AHT20_CMD_INIT_2) {
        m->init_sent = true;
        m->calibrated = true;
        m->init_count++;
        return true;
    }

    /* 3-byte trigger command: [0xAC, 0x33, 0x00] */
    if (len >= 3 &&
        data[0] == AHT20_CMD_TRIGGER_0 &&
        data[1] == AHT20_CMD_TRIGGER_1 &&
        data[2] == AHT20_CMD_TRIGGER_2) {
        m->measurement_pending = true;
        m->measure_count++;
        return true;
    }

    /* 1-byte soft reset: [0xBA] */
    if (data[0] == AHT20_CMD_SOFT_RESET) {
        m->reset_count++;
        m->measurement_pending = false;
        return true;
    }

    return true;
}

__attribute__((unused))
static bool aht20_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    aht20_model_t *m = (aht20_model_t *)ctx;

    /* 1-byte status read */
    if (len == 1) {
        uint8_t status = 0x00;
        if (m->calibrated)
            status |= AHT20_STATUS_CAL;
        /* Not busy (measurement complete) */
        data[0] = status;
        return true;
    }

    /* 7-byte measurement read:
     * [status, hum[19:12], hum[11:4], hum[3:0]|temp[19:16],
     *  temp[15:8], temp[7:0], CRC-8] */
    if (len >= 7) {
        uint8_t status = 0x00;
        if (m->calibrated)
            status |= AHT20_STATUS_CAL;
        /* Not busy */
        data[0] = status;

        uint32_t h_raw = aht20_model_hum_to_raw(m->true_humidity_pct);
        uint32_t t_raw = aht20_model_temp_to_raw(m->true_temp_c);

        data[1] = (uint8_t)((h_raw >> 12) & 0xFF);
        data[2] = (uint8_t)((h_raw >> 4) & 0xFF);
        data[3] = (uint8_t)(((h_raw & 0x0F) << 4) | ((t_raw >> 16) & 0x0F));
        data[4] = (uint8_t)((t_raw >> 8) & 0xFF);
        data[5] = (uint8_t)(t_raw & 0xFF);
        data[6] = aht20_model_crc8(data, 6);

        m->measurement_pending = false;
        return true;
    }

    memset(data, 0xFF, len);
    return true;
}

/* Delay callback (no-op for model) */
__attribute__((unused))
static void aht20_model_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

static void aht20_model_init(aht20_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Default environment: 25C, 50% RH, calibrated */
    m->true_temp_c = 25.0f;
    m->true_humidity_pct = 50.0f;
    m->calibrated = true;
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

#endif /* HW_MODEL_AHT20_H */
