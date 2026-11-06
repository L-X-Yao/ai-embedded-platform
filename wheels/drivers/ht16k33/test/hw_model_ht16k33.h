// SPDX-License-Identifier: MIT
/*
 * hw_model_ht16k33.h -- HT16K33 hardware register model from datasheet
 *
 * Models HT16K33 behavior, Revision V1.10, May 16, 2011:
 * - Display RAM: 16 bytes at addr 0x00-0x0F (R/W), read pointer wraps to 0x00
 * - Key data RAM: 6 bytes at addr 0x40-0x45 (read only), wraps to 0x40
 * - INT flag: addr 0x60 (read only), the flag in all eight bits
 * - Commands: system setup 0010 XXXS (0x20-0x2F), display setup 1000 X B1 B0 D
 *   (0x80-0x8F), dimming 1110 P3-P0 (0xE0-0xEF)
 * - ROW/INT set (0xA0-0xAF) is accepted and has no effect here
 * - Standby accepts only system setup (p.11)
 *
 * POR defaults, p.11:
 *   Oscillator = off
 *   Display = off
 *   Brightness = 15 (16/16 duty)
 *
 * The manual gives no display RAM state at power-on. It reads as zeros here,
 * which is this model's choice. It also does not say that a data byte after a
 * command is ignored, as this model's write_reg does: the HAL has no command-only
 * write, so the byte the driver sends after each command is not checked.
 */
#ifndef HW_MODEL_HT16K33_H
#define HW_MODEL_HT16K33_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t display_ram[16]; /* addr 0x00-0x0F */
    uint8_t key_ram[6];      /* addr 0x40-0x45 */
    bool    int_flag;        /* addr 0x60 */
    bool    oscillator_on;
    bool    display_on;
    uint8_t blink;           /* B1,B0 */
    uint8_t brightness;      /* 0-15 */
    int     write_count;
    int     read_count;
    int     ignored_in_standby;
    uint8_t last_cmd;        /* last command byte received */
} ht16k33_model_t;

static ht16k33_model_t ht16k33_mdl;

static void hw_model_clear_keys(void)
{
    memset(ht16k33_mdl.key_ram, 0, sizeof(ht16k33_mdl.key_ram));
    ht16k33_mdl.int_flag = false;
}

static void hw_model_reset(void)
{
    memset(&ht16k33_mdl, 0, sizeof(ht16k33_mdl));
    /* p.11: "Dimming is set to 16/16duty" */
    ht16k33_mdl.brightness = 15;
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    ht16k33_mdl.write_count++;
    ht16k33_mdl.last_cmd = reg;

    /* System setup: 0x20-0x2F, bit0 = S, bits 3:1 are X (p.33) */
    if ((reg & 0xF0u) == 0x20u) {
        ht16k33_mdl.oscillator_on = (reg & 0x01u) != 0;
        if (!ht16k33_mdl.oscillator_on) {
            /* p.11: the display goes off, key data and INT flags are cleared */
            ht16k33_mdl.display_on = false;
            hw_model_clear_keys();
        }
        return true;
    }

    /* p.11: "In the standby mode, the HT16K33 can not accept input commands nor
     * write data to the display RAM except using the system setup command." */
    if (!ht16k33_mdl.oscillator_on) {
        ht16k33_mdl.ignored_in_standby++;
        return true;
    }

    /* Display RAM range: 0x00-0x0F */
    if (reg <= 0x0Fu) {
        ht16k33_mdl.display_ram[reg] = val;
        return true;
    }

    /* Display setup: 0x80-0x8F, bit0 = D, bits 2:1 = blink, bit3 is X (p.33) */
    if ((reg & 0xF0u) == 0x80u) {
        ht16k33_mdl.display_on = (reg & 0x01u) != 0;
        ht16k33_mdl.blink = (uint8_t)((reg >> 1) & 0x03u);
        return true;
    }

    /* Brightness: 0xE0-0xEF (low nibble = duty) */
    if ((reg & 0xF0u) == 0xE0u) {
        ht16k33_mdl.brightness = (uint8_t)(reg & 0x0Fu);
        return true;
    }

    /* p.33: "If programmed command data is not defined, the function will not
     * be affected." p.24: "A write to address 0x40~0x45 is ignored." */
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ht16k33_mdl.read_count++;

    for (uint16_t i = 0; i < len; i++) {
        if (reg <= 0x0Fu) {
            /* p.33: past 0x0F the pointer goes back to 0x00 */
            buf[i] = ht16k33_mdl.display_ram[(reg + i) % 16u];
        } else if (reg >= 0x40u && reg <= 0x45u) {
            /* p.33: past 0x45 the pointer goes back to 0x40 */
            buf[i] = ht16k33_mdl.key_ram[((unsigned)reg - 0x40u + i) % 6u];
        } else if (reg == 0x60u) {
            /* p.21: every bit of the register is the INT flag */
            buf[i] = ht16k33_mdl.int_flag ? 0xFFu : 0x00u;
        } else {
            buf[i] = 0x00; /* the manual defines no other read */
        }
    }

    /* p.21: "after all the key data has been read that clears the key data RAM
     * and the int flag bit is set to 0". p.33 wants it in one read from 0x40. */
    if (reg == 0x40u && len >= 6u) {
        hw_model_clear_keys();
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* The key scan finds these keys: key data RAM fills and the INT flag rises */
__attribute__((unused))
static void hw_model_set_keys(const uint8_t keys[6])
{
    memcpy(ht16k33_mdl.key_ram, keys, sizeof(ht16k33_mdl.key_ram));
    ht16k33_mdl.int_flag = true;
}

#endif /* HW_MODEL_HT16K33_H */
