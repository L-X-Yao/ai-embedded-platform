/*
 * hw_model_shtc3.h — datasheet-accurate command model for SHTC3
 *
 * Provides write/read callbacks simulating the SHTC3 command protocol.
 * SHTC3 uses 16-bit commands (not registers). The model tracks state
 * (sleep/awake) and responds with appropriate data.
 */
#ifndef HW_MODEL_SHTC3_H
#define HW_MODEL_SHTC3_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Forward-declare CRC function from shtc3.h */
uint8_t shtc3_crc8(const uint8_t *data, uint16_t len);

static int     hw_write_count;
static int     hw_read_count;
static bool    hw_awake;
static uint8_t hw_last_cmd_msb;
static uint8_t hw_last_cmd_lsb;

/* Injected raw measurement values */
static uint16_t hw_raw_temp;
static uint16_t hw_raw_hum;

static void hw_model_reset(void)
{
    hw_write_count  = 0;
    hw_read_count   = 0;
    hw_awake        = false;  /* SHTC3 starts in sleep mode */
    hw_last_cmd_msb = 0;
    hw_last_cmd_lsb = 0;
    hw_raw_temp     = 26214;  /* ~25 C */
    hw_raw_hum      = 32768;  /* ~50% */
}

__attribute__((unused))
static bool hw_model_write(void *ctx, uint8_t dev_addr,
                           const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    if (len >= 2)
    {
        hw_last_cmd_msb = data[0];
        hw_last_cmd_lsb = data[1];

        /* Track wakeup/sleep state */
        if (data[0] == 0x35 && data[1] == 0x17)
        {
            hw_awake = true;
        }
        else if (data[0] == 0xB0 && data[1] == 0x98)
        {
            hw_awake = false;
        }
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read(void *ctx, uint8_t dev_addr,
                          uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;

    if (hw_last_cmd_msb == 0xEF && hw_last_cmd_lsb == 0xC8)
    {
        /* Read ID command response: 0x0807 with CRC */
        if (len >= 3)
        {
            data[0] = 0x08;
            data[1] = 0x07;
            data[2] = shtc3_crc8(&data[0], 2);
        }
    }
    else if ((hw_last_cmd_msb == 0x78 && hw_last_cmd_lsb == 0x66) ||
             (hw_last_cmd_msb == 0x60 && hw_last_cmd_lsb == 0x9C))
    {
        /* Measurement response: temp + CRC + hum + CRC */
        if (len >= 6)
        {
            data[0] = (uint8_t)(hw_raw_temp >> 8);
            data[1] = (uint8_t)(hw_raw_temp & 0xFF);
            data[2] = shtc3_crc8(&data[0], 2);
            data[3] = (uint8_t)(hw_raw_hum >> 8);
            data[4] = (uint8_t)(hw_raw_hum & 0xFF);
            data[5] = shtc3_crc8(&data[3], 2);
        }
    }
    else
    {
        memset(data, 0xFF, len);
    }

    return true;
}

__attribute__((unused))
static void hw_model_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

__attribute__((unused))
static void hw_model_inject_reading(uint16_t raw_temp, uint16_t raw_hum)
{
    hw_raw_temp = raw_temp;
    hw_raw_hum  = raw_hum;
}

#endif /* HW_MODEL_SHTC3_H */
