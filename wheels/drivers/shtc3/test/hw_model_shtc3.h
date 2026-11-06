// SPDX-License-Identifier: MIT
/*
 * hw_model_shtc3.h — datasheet-accurate command model for SHTC3
 *
 * Provides write/read callbacks simulating the SHTC3 command protocol.
 * SHTC3 uses 16-bit commands (not registers). The model tracks state
 * (sleep/awake) and responds with appropriate data.
 *
 * Time passes through hw_model_delay. As in the manual:
 * - the sensor powers up idle (p.4)
 * - asleep it takes nothing but the Wakeup command (p.7)
 * - it answers nothing during a measurement (p.9), which takes tMEAS (Table 5)
 * - the Wakeup and the soft reset take tPU and tSR (Table 5). The manual does not
 *   say what a command meets inside those times, so the model refuses it
 * Table 5 gives typical times only: its maximums are tbd in this version.
 */
#ifndef HW_MODEL_SHTC3_H
#define HW_MODEL_SHTC3_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../shtc3.h"

static int     hw_write_count;
static int     hw_read_count;
static bool    hw_awake;
static uint8_t hw_last_cmd_msb;
static uint8_t hw_last_cmd_lsb;
static int      hw_reset_count;
static uint32_t hw_now_us;
static uint32_t hw_idle_at_us;   /* the sensor is busy until then */

/* Table 5, p.5, typical */
#define HW_TPU_US    230u
#define HW_TSR_US    173u
#define HW_TMEAS_US  10800u

/* Injected raw measurement values */
static uint16_t hw_raw_temp;
static uint16_t hw_raw_hum;

static void hw_model_reset(void)
{
    hw_write_count  = 0;
    hw_read_count   = 0;
    hw_awake        = true;   /* p.4: idle after power-up, until a Sleep command */
    hw_last_cmd_msb = 0;
    hw_last_cmd_lsb = 0;
    hw_reset_count  = 0;
    hw_now_us       = 0;
    hw_idle_at_us   = 0;
    hw_raw_temp     = 26214;  /* ~25 C */
    hw_raw_hum      = 32768;  /* ~50% */
}

__attribute__((unused))
static bool hw_model_write(void *ctx, uint8_t dev_addr,
                           const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    if (len < 2)
    {
        return true;
    }

    /* p.9: "the sensor does not respond to any I2C activity during measurement" */
    if (hw_now_us < hw_idle_at_us)
    {
        return false;
    }

    bool wakeup = data[0] == 0x35 && data[1] == 0x17;

    /* p.7: asleep it "requires the following wake up command before any further
     * communication" */
    if (!hw_awake && !wakeup)
    {
        return false;
    }

    hw_last_cmd_msb = data[0];
    hw_last_cmd_lsb = data[1];

    if (wakeup)
    {
        if (!hw_awake)
        {
            hw_awake = true;
            hw_idle_at_us = hw_now_us + HW_TPU_US;
        }
    }
    else if (data[0] == 0xB0 && data[1] == 0x98)
    {
        hw_awake = false;
    }
    else if (data[0] == 0x80 && data[1] == 0x5D)
    {
        hw_reset_count++;
        hw_idle_at_us = hw_now_us + HW_TSR_US;
    }
    else if ((data[0] == 0x78 && data[1] == 0x66) ||
             (data[0] == 0x60 && data[1] == 0x9C))
    {
        hw_idle_at_us = hw_now_us + HW_TMEAS_US;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read(void *ctx, uint8_t dev_addr,
                          uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;

    uint8_t  resp[6];
    uint16_t resp_len;

    /* p.7 and p.9, as for a write */
    if (!hw_awake || hw_now_us < hw_idle_at_us)
    {
        return false;
    }

    if (hw_last_cmd_msb == 0xEF && hw_last_cmd_lsb == 0xC8)
    {
        /* Read ID command response: 0x0807 with CRC */
        resp[0] = 0x08;
        resp[1] = 0x07;
        resp[2] = shtc3_crc8(&resp[0], 2);
        resp_len = 3;
    }
    else if ((hw_last_cmd_msb == 0x78 && hw_last_cmd_lsb == 0x66) ||
             (hw_last_cmd_msb == 0x60 && hw_last_cmd_lsb == 0x9C))
    {
        /* Measurement response: temp + CRC + hum + CRC */
        resp[0] = (uint8_t)(hw_raw_temp >> 8);
        resp[1] = (uint8_t)(hw_raw_temp & 0xFF);
        resp[2] = shtc3_crc8(&resp[0], 2);
        resp[3] = (uint8_t)(hw_raw_hum >> 8);
        resp[4] = (uint8_t)(hw_raw_hum & 0xFF);
        resp[5] = shtc3_crc8(&resp[3], 2);
        resp_len = 6;
    }
    else
    {
        return false;   /* no command waits for a read */
    }

    /* p.9: the master can abort the transfer "after any data byte" */
    memset(data, 0xFF, len);
    memcpy(data, resp, len < resp_len ? len : resp_len);
    hw_last_cmd_msb = 0;
    hw_last_cmd_lsb = 0;
    return true;
}

static void hw_model_advance_us(uint32_t us)
{
    hw_now_us += us;
}

__attribute__((unused))
static void hw_model_delay(void *ctx, uint16_t ms)
{
    (void)ctx;
    hw_model_advance_us((uint32_t)ms * 1000u);
}

__attribute__((unused))
static void hw_model_inject_reading(uint16_t raw_temp, uint16_t raw_hum)
{
    hw_raw_temp = raw_temp;
    hw_raw_hum  = raw_hum;
}

#endif /* HW_MODEL_SHTC3_H */
