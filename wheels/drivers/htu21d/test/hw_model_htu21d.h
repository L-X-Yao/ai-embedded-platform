/*
 * hw_model_htu21d.h — HTU21D command-based hardware model
 *
 * The HTU21D is NOT register-based — it uses single-byte commands:
 * - Write 1-byte command (0xE3 temp, 0xE5 humidity, 0xFE reset)
 * - Read 3-byte response (MSB, LSB, CRC-8)
 *
 * This model tracks the last command and returns appropriate data
 * with correct CRC-8 (poly 0x131, init 0x00).
 */

#ifndef HW_MODEL_HTU21D_H
#define HW_MODEL_HTU21D_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- HTU21D commands (from datasheet) --- */
#define HW_HTU21D_CMD_TRIG_TEMP    0xE3u
#define HW_HTU21D_CMD_TRIG_HUM     0xE5u
#define HW_HTU21D_CMD_SOFT_RESET   0xFEu

/* --- Simulated environment --- */
typedef struct {
    float   true_temp_c;          /* simulated temperature */
    float   true_humidity_pct;    /* simulated humidity    */
    /* Command state */
    uint8_t last_cmd;
    int     temp_count;
    int     hum_count;
    int     reset_count;
} htu21d_model_t;

/* HTU21D CRC-8: poly 0x131 (applied as 0x31), init 0x00 */
static uint8_t htu21d_model_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
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

/*
 * Convert temperature C to HTU21D raw (14-bit, status bits cleared).
 * Formula: T = -46.85 + 175.72 * raw / 65536
 * Inverse: raw = (T + 46.85) / 175.72 * 65536
 * Status bits [1:0] of LSB are 0 for temperature.
 */
static uint16_t htu21d_model_temp_to_raw(float temp_c)
{
    float norm = (temp_c + 46.85f) / 175.72f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    uint16_t raw = (uint16_t)(norm * 65536.0f);
    raw &= 0xFFFC; /* clear status bits */
    return raw;
}

/*
 * Convert humidity % to HTU21D raw (12-bit, status bits cleared).
 * Formula: RH = -6 + 125 * raw / 65536
 * Inverse: raw = (RH + 6) / 125 * 65536
 * Status bits [1:0] of LSB are 0x02 for humidity, but we clear for CRC calc.
 */
static uint16_t htu21d_model_hum_to_raw(float humidity_pct)
{
    float norm = (humidity_pct + 6.0f) / 125.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    uint16_t raw = (uint16_t)(norm * 65536.0f);
    raw &= 0xFFFC; /* clear status bits */
    return raw;
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool htu21d_model_write_cmd(void *ctx, uint8_t dev_addr, uint8_t cmd)
{
    (void)dev_addr;
    htu21d_model_t *m = (htu21d_model_t *)ctx;

    m->last_cmd = cmd;

    if (cmd == HW_HTU21D_CMD_TRIG_TEMP) {
        m->temp_count++;
    } else if (cmd == HW_HTU21D_CMD_TRIG_HUM) {
        m->hum_count++;
    } else if (cmd == HW_HTU21D_CMD_SOFT_RESET) {
        m->reset_count++;
    }

    return true;
}

__attribute__((unused))
static bool htu21d_model_read_data(void *ctx, uint8_t dev_addr,
                                   uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    htu21d_model_t *m = (htu21d_model_t *)ctx;

    if (len < 3) {
        memset(buf, 0xFF, len);
        return true;
    }

    uint16_t raw;
    if (m->last_cmd == HW_HTU21D_CMD_TRIG_TEMP) {
        raw = htu21d_model_temp_to_raw(m->true_temp_c);
    } else if (m->last_cmd == HW_HTU21D_CMD_TRIG_HUM) {
        raw = htu21d_model_hum_to_raw(m->true_humidity_pct);
    } else {
        memset(buf, 0xFF, len);
        return true;
    }

    buf[0] = (uint8_t)(raw >> 8);
    buf[1] = (uint8_t)(raw & 0xFF);
    buf[2] = htu21d_model_crc8(buf, 2);

    return true;
}

/* Delay callback (no-op for model) */
__attribute__((unused))
static void htu21d_model_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static void htu21d_model_init(htu21d_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Default environment: 25C, 50% RH */
    m->true_temp_c       = 25.0f;
    m->true_humidity_pct = 50.0f;
}

/* Helper: set simulated temperature */
__attribute__((unused))
static void htu21d_model_set_temperature(htu21d_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

/* Helper: set simulated humidity */
__attribute__((unused))
static void htu21d_model_set_humidity(htu21d_model_t *m, float humidity_pct)
{
    m->true_humidity_pct = humidity_pct;
}

#endif /* HW_MODEL_HTU21D_H */
