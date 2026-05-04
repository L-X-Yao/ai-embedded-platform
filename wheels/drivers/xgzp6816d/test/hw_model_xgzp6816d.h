/*
 * hw_model_xgzp6816d.h — XGZP6816D hardware model
 *
 * Models XGZP6816D protocol behavior:
 * - Probe: zero-length write (address ACK)
 * - Trigger: write 0xAC command byte
 * - Read: 6 bytes (status + 24-bit pressure + 16-bit temperature)
 *
 * The XGZP6816D is not register-based; it uses command-response I2C.
 * This model uses hw_reg_model_t as backing storage but provides
 * custom write/read callbacks matching the driver's raw I2C HAL.
 */

#ifndef HW_MODEL_XGZP6816D_H
#define HW_MODEL_XGZP6816D_H

#include "hw_model.h"

#define XGZP_CMD_MEASURE_PT  0xACu
#define XGZP_DEV_ADDR        0x78u

/* Pressure calibration constants (from driver) */
#define XGZP_P_RAW_MIN    1677722.0f
#define XGZP_P_RAW_MAX   15099494.0f
#define XGZP_P_KPA_MIN   30.0f
#define XGZP_P_KPA_MAX  110.0f

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;             /* not really used for registers */
    float            true_pressure_pa;   /* simulated pressure in Pa */
    float            true_temp_c;        /* simulated temperature in C */
    bool             triggered;          /* trigger command received */
    int              trigger_count;
    int              read_count;
} xgzp6816d_model_t;

/* Convert pressure Pa to 24-bit raw ADC */
static uint32_t xgzp_pressure_to_raw(float pressure_pa)
{
    float p_kpa = pressure_pa / 1000.0f;
    if (p_kpa < XGZP_P_KPA_MIN) p_kpa = XGZP_P_KPA_MIN;
    if (p_kpa > XGZP_P_KPA_MAX) p_kpa = XGZP_P_KPA_MAX;
    float raw = (p_kpa - XGZP_P_KPA_MIN) / (XGZP_P_KPA_MAX - XGZP_P_KPA_MIN)
              * (XGZP_P_RAW_MAX - XGZP_P_RAW_MIN) + XGZP_P_RAW_MIN;
    return (uint32_t)(raw + 0.5f);
}

/* Convert temperature C to 16-bit raw: t_raw = (C + 40) / 190 * 65536 */
static uint16_t xgzp_temp_to_raw(float temp_c)
{
    float norm = (temp_c + 40.0f) / 190.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65536.0f);
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool xgzp6816d_model_write(void *ctx, uint8_t dev_addr,
                                  const uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    xgzp6816d_model_t *m = (xgzp6816d_model_t *)ctx;

    /* Zero-length write = probe */
    if (len == 0 || buf == NULL)
        return true;

    /* Trigger command */
    if (len == 1 && buf[0] == XGZP_CMD_MEASURE_PT) {
        m->triggered = true;
        m->trigger_count++;
    }

    return true;
}

__attribute__((unused))
static bool xgzp6816d_model_read(void *ctx, uint8_t dev_addr,
                                 uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    xgzp6816d_model_t *m = (xgzp6816d_model_t *)ctx;

    if (len < 6) {
        memset(buf, 0, len);
        return true;
    }

    uint32_t p_raw = xgzp_pressure_to_raw(m->true_pressure_pa);
    uint16_t t_raw = xgzp_temp_to_raw(m->true_temp_c);

    /* Layout: status(1) + pressure(3) + temperature(2) */
    buf[0] = 0x00;  /* status: conversion complete */
    buf[1] = (uint8_t)(p_raw >> 16);
    buf[2] = (uint8_t)(p_raw >> 8);
    buf[3] = (uint8_t)(p_raw & 0xFF);
    buf[4] = (uint8_t)(t_raw >> 8);
    buf[5] = (uint8_t)(t_raw & 0xFF);

    m->read_count++;
    return true;
}

static void xgzp6816d_model_init(xgzp6816d_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Default environment: ~101325 Pa (1 atm), 25C */
    m->true_pressure_pa = 101325.0f;
    m->true_temp_c = 25.0f;
    m->triggered = false;
    m->trigger_count = 0;
    m->read_count = 0;

    hw_model_reset(&m->hw);
}

/* Helper: set simulated pressure (Pa) */
static void xgzp6816d_model_set_pressure(xgzp6816d_model_t *m, float pressure_pa)
{
    m->true_pressure_pa = pressure_pa;
}

/* Helper: set simulated temperature (C) */
static void xgzp6816d_model_set_temperature(xgzp6816d_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

#endif /* HW_MODEL_XGZP6816D_H */
