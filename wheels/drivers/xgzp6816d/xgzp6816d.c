/*
 * xgzp6816d — XGZP6816D barometric pressure sensor driver (I2C)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal
 */

#include "xgzp6816d.h"
#include <stddef.h>

/* ---------- protocol constants ---------- */

#define CMD_MEASURE_PT     0xACu   /* trigger combined pressure + temperature conversion */

/* ---------- pressure calibration (datasheet 30–110 kPa range) ----------
 *
 * The 24-bit ADC counts span the rated pressure range linearly:
 *   p_raw =  1677722 ↔  30 kPa  (low rail)
 *   p_raw = 15099494 ↔ 110 kPa  (high rail)
 *
 * Outside this range the part is out of spec; we clamp to avoid feeding
 * downstream filters wildly wrong values when a sensor is disconnected
 * or saturated. (The pre-wheel flybox driver only clamped low; high-side
 * clamp is a behavior improvement.)
 */
#define P_RAW_MIN    1677722.0f
#define P_RAW_MAX   15099494.0f
#define P_KPA_MIN   30.0f
#define P_KPA_MAX  110.0f

/* ---------- API ---------- */

xgzp6816d_err_t xgzp6816d_probe(const xgzp6816d_i2c_t *i2c)
{
    /* Zero-byte write — START + addr_W + STOP. The HAL layer is expected to
     * report failure if the device NACKs the address; that's the only signal
     * we have at this stage (no chip-id register on the XGZP6816D). */
    return i2c->write(i2c->ctx, XGZP6816D_DEV_ADDR, NULL, 0)
           ? XGZP6816D_OK : XGZP6816D_ERR_NO_ACK;
}

xgzp6816d_err_t xgzp6816d_trigger(const xgzp6816d_i2c_t *i2c)
{
    uint8_t cmd = CMD_MEASURE_PT;
    return i2c->write(i2c->ctx, XGZP6816D_DEV_ADDR, &cmd, 1)
           ? XGZP6816D_OK : XGZP6816D_ERR_I2C;
}

xgzp6816d_err_t xgzp6816d_read(const xgzp6816d_i2c_t *i2c,
                                xgzp6816d_data_t *out)
{
    uint8_t buf[6];
    if (!i2c->read(i2c->ctx, XGZP6816D_DEV_ADDR, buf, 6)) {
        return XGZP6816D_ERR_I2C;
    }

    /* Layout per datasheet (after CMD_MEASURE_PT + tconv):
     *   buf[0] = status byte (busy/EOC, ignored)
     *   buf[1..3] = pressure 24-bit big-endian
     *   buf[4..5] = temperature 16-bit big-endian
     */
    uint32_t p_raw = ((uint32_t)buf[1] << 16)
                   | ((uint32_t)buf[2] << 8)
                   |  (uint32_t)buf[3];
    uint16_t t_raw = ((uint16_t)buf[4] << 8) | buf[5];

    float p_kpa;
    if ((float)p_raw <= P_RAW_MIN) {
        p_kpa = P_KPA_MIN;
    } else if ((float)p_raw >= P_RAW_MAX) {
        p_kpa = P_KPA_MAX;
    } else {
        p_kpa = (P_KPA_MAX - P_KPA_MIN) / (P_RAW_MAX - P_RAW_MIN)
              * ((float)p_raw - P_RAW_MIN) + P_KPA_MIN;
    }
    out->pressure_pa = p_kpa * 1000.0f;
    out->temperature_c = (float)t_raw / 65536.0f * 190.0f - 40.0f;

    return XGZP6816D_OK;
}
