// SPDX-License-Identifier: MIT
/*
 * hw_model_max6675.h — MAX6675 SPI model
 *
 * MAX6675 is read-only: each CS cycle returns a fixed 16-bit frame.
 * Bit layout: D15=dummy, D14..D3=12-bit temp, D2=open, D1=ID(0), D0=tri-state
 */
#ifndef HW_MODEL_MAX6675_H
#define HW_MODEL_MAX6675_H

#include <stdint.h>
#include <stdbool.h>

static uint16_t hw_frame;
static int      hw_byte_idx;
static int      hw_read_count;

static void hw_model_reset(void)
{
    hw_frame = 0;
    hw_byte_idx = 0;
    hw_read_count = 0;
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx)
{
    (void)ctx;
    hw_byte_idx = 0;
    hw_read_count++;
}

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx; (void)tx;
    uint8_t ret;
    if (hw_byte_idx == 0)
        ret = (uint8_t)(hw_frame >> 8);
    else
        ret = (uint8_t)(hw_frame & 0xFF);
    hw_byte_idx++;
    return ret;
}

static void hw_model_inject_temperature(float temp_c)
{
    uint16_t counts = (uint16_t)(temp_c / 0.25f);
    hw_frame = (uint16_t)(counts << 3);
}

static void hw_model_set_open(bool open)
{
    if (open)
        hw_frame |= (1u << 2);
    else
        hw_frame &= (uint16_t)~(1u << 2);
}

#endif /* HW_MODEL_MAX6675_H */
