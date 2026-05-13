/*
 * hw_model_ht16k33.h -- HT16K33 hardware register model from datasheet
 *
 * Models HT16K33 behavior:
 * - Display RAM: 16 bytes at addr 0x00-0x0F (R/W)
 * - Commands: oscillator on/off (0x20-0x21), display on/off (0x80-0x83),
 *   brightness (0xE0-0xEF)
 * - No readable status registers (command-only chip)
 *
 * POR defaults:
 *   Oscillator = off
 *   Display = off
 *   Brightness = 0 (1/16 duty)
 *   Display RAM = all zeros
 */
#ifndef HW_MODEL_HT16K33_H
#define HW_MODEL_HT16K33_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t display_ram[16]; /* addr 0x00-0x0F */
    bool    oscillator_on;
    bool    display_on;
    uint8_t brightness;      /* 0-15 */
    int     write_count;
    int     read_count;
    uint8_t last_cmd;        /* last command byte received */
} ht16k33_model_t;

static ht16k33_model_t ht16k33_mdl;

static void hw_model_reset(void)
{
    memset(&ht16k33_mdl, 0, sizeof(ht16k33_mdl));
    /* POR defaults: oscillator off, display off, brightness 0, RAM zeroed */
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    ht16k33_mdl.write_count++;
    ht16k33_mdl.last_cmd = reg;

    /* Display RAM range: 0x00-0x0F */
    if (reg <= 0x0Fu) {
        ht16k33_mdl.display_ram[reg] = val;
        return true;
    }

    /* System oscillator command: 0x20 (off) / 0x21 (on) */
    if ((reg & 0xFEu) == 0x20u) {
        ht16k33_mdl.oscillator_on = (reg & 0x01u) != 0;
        return true;
    }

    /* Display on/off + blink: 0x80-0x83 (bit0 = on, bit2:1 = blink) */
    if ((reg & 0xF8u) == 0x80u) {
        ht16k33_mdl.display_on = (reg & 0x01u) != 0;
        return true;
    }

    /* Brightness: 0xE0-0xEF (low nibble = duty) */
    if ((reg & 0xF0u) == 0xE0u) {
        ht16k33_mdl.brightness = (uint8_t)(reg & 0x0Fu);
        return true;
    }

    /* Unknown command — ignored (real chip ignores unrecognized bytes) */
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ht16k33_mdl.read_count++;

    /* Only display RAM is readable (0x00-0x0F) */
    for (uint16_t i = 0; i < len; i++) {
        uint8_t addr = (uint8_t)(reg + i);
        if (addr <= 0x0Fu) {
            buf[i] = ht16k33_mdl.display_ram[addr];
        } else {
            buf[i] = 0x00; /* non-RAM addresses return 0 */
        }
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_HT16K33_H */
