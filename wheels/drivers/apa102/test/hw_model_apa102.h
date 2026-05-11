// SPDX-License-Identifier: MIT
/*
 * hw_model_apa102.h — APA102 LED strip SPI model
 *
 * Records SPI byte stream and validates the APA102 frame protocol:
 * start frame, LED frames, end frame.
 */
#ifndef HW_MODEL_APA102_H
#define HW_MODEL_APA102_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_XFER_LOG_SIZE 1024

static uint8_t hw_xfer_log[HW_XFER_LOG_SIZE];
static int     hw_xfer_count;
static int     hw_cs_state;

static void hw_model_reset(void)
{
    memset(hw_xfer_log, 0, sizeof(hw_xfer_log));
    hw_xfer_count = 0;
    hw_cs_state   = 1;
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx) { (void)ctx; hw_cs_state = 0; }

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (hw_xfer_count < HW_XFER_LOG_SIZE)
        hw_xfer_log[hw_xfer_count++] = tx;
    return 0x00;
}

/* Validate that the SPI log contains a valid APA102 frame for n LEDs.
 * Returns 1 if valid, 0 otherwise. */
__attribute__((unused))
static int hw_model_validate_frame(int n_leds)
{
    int idx = 0;

    /* Start frame: 4 bytes of 0x00 */
    if (hw_xfer_count < 4) return 0;
    for (int i = 0; i < 4; i++) {
        if (hw_xfer_log[idx++] != 0x00) return 0;
    }

    /* LED frames: each starts with 0xE0-0xFF */
    for (int i = 0; i < n_leds; i++) {
        if (idx + 4 > hw_xfer_count) return 0;
        if ((hw_xfer_log[idx] & 0xE0) != 0xE0) return 0;
        idx += 4;
    }

    /* End frame: remaining bytes should be 0xFF */
    while (idx < hw_xfer_count) {
        if (hw_xfer_log[idx++] != 0xFF) return 0;
    }

    return 1;
}

#endif /* HW_MODEL_APA102_H */
