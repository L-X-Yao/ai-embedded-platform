/*
 * hw_model_bmi270.h — datasheet-accurate SPI register model for BMI270
 *
 * BMI270 SPI protocol:
 *   Write: addr byte (bit7=0) + data byte
 *   Read:  addr byte (bit7=1) + dummy byte + data byte(s)
 *
 * Tracks the byte index within a transaction to handle the dummy byte
 * correctly for read operations.
 */
#ifndef HW_MODEL_BMI270_H
#define HW_MODEL_BMI270_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_xfer_count;
static int     hw_cs_state;
static uint8_t hw_addr_latch;
static int     hw_byte_idx;
static bool    hw_is_read;

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_xfer_count = 0;
    hw_cs_state   = 1;
    hw_addr_latch = 0;
    hw_byte_idx   = 0;
    hw_is_read    = false;

    /* POR defaults from BMI270 datasheet */
    hw_regs[0x00] = 0x24;  /* CHIP_ID */
    hw_regs[0x02] = 0x00;  /* ERR_REG: no errors */
    hw_regs[0x03] = 0x00;  /* STATUS */
    hw_regs[0x21] = 0x01;  /* INTERNAL_STATUS: init OK */

    /* Default accel config: normal BWP, 100Hz ODR */
    hw_regs[0x40] = 0x28;  /* ACC_CONF */
    hw_regs[0x41] = 0x00;  /* ACC_RANGE: +/-2g */
    /* Default gyro config: normal BWP, 200Hz ODR */
    hw_regs[0x42] = 0x29;  /* GYR_CONF */
    hw_regs[0x43] = 0x00;  /* GYR_RANGE: +/-2000dps */

    hw_regs[0x7C] = 0x00;  /* PWR_CONF */
    hw_regs[0x7D] = 0x00;  /* PWR_CTRL */
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx)
{
    (void)ctx;
    hw_cs_state = 0;
    hw_byte_idx = 0;
}

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    hw_xfer_count++;

    if (hw_byte_idx == 0) {
        /* Address byte */
        hw_is_read = (tx & 0x80u) != 0;
        hw_addr_latch = (uint8_t)(tx & 0x7Fu);
        hw_byte_idx++;
        return 0x00;
    }

    if (hw_is_read) {
        if (hw_byte_idx == 1) {
            /* Dummy byte for read operations */
            hw_byte_idx++;
            return 0x00;
        }
        /* Data bytes */
        uint8_t offset = (uint8_t)(hw_byte_idx - 2);
        uint8_t reg = (uint8_t)(hw_addr_latch + offset);
        hw_byte_idx++;
        return hw_regs[reg];
    } else {
        /* Write: byte 1+ are data */
        if (hw_byte_idx == 1) {
            hw_regs[hw_addr_latch] = tx;
        } else {
            uint8_t offset = (uint8_t)(hw_byte_idx - 1);
            uint8_t reg = (uint8_t)(hw_addr_latch + offset);
            hw_regs[reg] = tx;
        }
        hw_byte_idx++;
        return 0x00;
    }
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* Inject raw accel+gyro data (little-endian 16-bit signed) */
__attribute__((unused))
static void hw_model_inject_accel_gyro(int16_t ax, int16_t ay, int16_t az,
                                        int16_t gx, int16_t gy, int16_t gz)
{
    /* ACC: 0x0C-0x11 (LE) */
    hw_regs[0x0C] = (uint8_t)(ax & 0xFFu);
    hw_regs[0x0D] = (uint8_t)((uint16_t)ax >> 8);
    hw_regs[0x0E] = (uint8_t)(ay & 0xFFu);
    hw_regs[0x0F] = (uint8_t)((uint16_t)ay >> 8);
    hw_regs[0x10] = (uint8_t)(az & 0xFFu);
    hw_regs[0x11] = (uint8_t)((uint16_t)az >> 8);
    /* GYR: 0x12-0x17 (LE) */
    hw_regs[0x12] = (uint8_t)(gx & 0xFFu);
    hw_regs[0x13] = (uint8_t)((uint16_t)gx >> 8);
    hw_regs[0x14] = (uint8_t)(gy & 0xFFu);
    hw_regs[0x15] = (uint8_t)((uint16_t)gy >> 8);
    hw_regs[0x16] = (uint8_t)(gz & 0xFFu);
    hw_regs[0x17] = (uint8_t)((uint16_t)gz >> 8);
}

/* Inject temperature raw value (LE) */
__attribute__((unused))
static void hw_model_inject_temp(int16_t raw_temp)
{
    hw_regs[0x22] = (uint8_t)(raw_temp & 0xFFu);
    hw_regs[0x23] = (uint8_t)((uint16_t)raw_temp >> 8);
}

#endif /* HW_MODEL_BMI270_H */
