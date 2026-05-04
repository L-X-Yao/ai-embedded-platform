/*
 * hw_model_max31855.h -- MAX31855 hardware model from datasheet
 *
 * Models Maxim MAX31855 read-only 32-bit SPI frame:
 *   Bits [31:18] = thermocouple temp (14-bit signed, 0.25 C/LSB)
 *   Bit  [16]    = fault flag
 *   Bits [15:4]  = internal temp (12-bit signed, 0.0625 C/LSB)
 *   Bit  [2]     = SCV (short to VCC)
 *   Bit  [1]     = SCG (short to GND)
 *   Bit  [0]     = OC  (open circuit)
 *
 * No register map -- just a 32-bit shift register read via 4 SPI bytes.
 */

#ifndef HW_MODEL_MAX31855_H
#define HW_MODEL_MAX31855_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint32_t frame;      /* the 32-bit SPI frame to return */
    int      read_count; /* number of reads performed */
} max31855_model_t;

/* Encode thermocouple temperature in 14-bit signed format (0.25 C/LSB) */
static uint32_t max31855_encode_tc(float temp_c)
{
    int32_t raw = (int32_t)(temp_c / 0.25f);
    return ((uint32_t)(raw & 0x3FFF)) << 18;
}

/* Encode internal temperature in 12-bit signed format (0.0625 C/LSB) */
static uint32_t max31855_encode_internal(float temp_c)
{
    int32_t raw = (int32_t)(temp_c / 0.0625f);
    return ((uint32_t)(raw & 0x0FFF)) << 4;
}

static void max31855_model_init(max31855_model_t *m)
{
    memset(m, 0, sizeof(*m));
    /* Default: 25.0 C thermocouple, 23.0 C internal, no faults */
    m->frame = max31855_encode_tc(25.0f) | max31855_encode_internal(23.0f);
    m->read_count = 0;
}

/* Set thermocouple + internal temperatures, no fault */
static void max31855_model_set_temps(max31855_model_t *m,
                                     float tc_c, float internal_c)
{
    m->frame = max31855_encode_tc(tc_c) | max31855_encode_internal(internal_c);
}

/* Set fault condition */
static void max31855_model_set_fault(max31855_model_t *m,
                                     bool open_circuit, bool short_gnd,
                                     bool short_vcc)
{
    /* Start with current temps */
    uint32_t f = m->frame & ~0x10007u;  /* clear fault flag + fault bits */
    if (open_circuit) f |= 0x01;
    if (short_gnd)    f |= 0x02;
    if (short_vcc)    f |= 0x04;
    if (open_circuit || short_gnd || short_vcc) f |= (1u << 16);
    m->frame = f;
}

/* --- SPI HAL callbacks --- */

typedef struct {
    max31855_model_t *model;
    int               byte_idx;
} max31855_spi_ctx_t;

__attribute__((unused))
static void max31855_model_cs_low(void *ctx)
{
    max31855_spi_ctx_t *sc = (max31855_spi_ctx_t *)ctx;
    sc->byte_idx = 0;
}

__attribute__((unused))
static void max31855_model_cs_high(void *ctx)
{
    max31855_spi_ctx_t *sc = (max31855_spi_ctx_t *)ctx;
    if (sc->byte_idx >= 4) {
        sc->model->read_count++;
    }
}

__attribute__((unused))
static uint8_t max31855_model_xfer(void *ctx, uint8_t tx)
{
    (void)tx;
    max31855_spi_ctx_t *sc = (max31855_spi_ctx_t *)ctx;
    uint32_t frame = sc->model->frame;
    uint8_t r = (uint8_t)(frame >> (24 - 8 * sc->byte_idx));
    sc->byte_idx++;
    return r;
}

#endif /* HW_MODEL_MAX31855_H */
