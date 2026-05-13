/*
 * hw_model_is31fl3731.h — datasheet-accurate register model for IS31FL3731
 *
 * Models ISSI IS31FL3731 page-based register architecture:
 * - Command register 0xFD selects active page
 * - 8 frame pages (0-7) with LED control, blink, and PWM registers
 * - Function register page (0x0B) with config, shutdown, etc.
 */
#ifndef HW_MODEL_IS31FL3731_H
#define HW_MODEL_IS31FL3731_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_NUM_PAGES     12
#define HW_REGS_PER_PAGE 256

typedef struct {
    uint8_t pages[HW_NUM_PAGES][HW_REGS_PER_PAGE];
    uint8_t current_page;
    int     write_count;
    int     read_count;
    bool    in_shutdown;
} is31fl3731_model_t;

static void is31fl3731_model_init(is31fl3731_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->current_page = 0;
    m->in_shutdown = true;  /* POR: shutdown mode */
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    is31fl3731_model_t *m = (is31fl3731_model_t *)ctx;
    m->write_count++;

    /* Command register: page select */
    if (reg == 0xFD) {
        m->current_page = val;
        return true;
    }

    /* Track shutdown state */
    if (m->current_page == 0x0B && reg == 0x0A) {
        m->in_shutdown = (val == 0x00);
    }

    if (m->current_page < HW_NUM_PAGES) {
        m->pages[m->current_page][reg] = val;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    is31fl3731_model_t *m = (is31fl3731_model_t *)ctx;
    m->read_count++;

    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        if (m->current_page < HW_NUM_PAGES && idx < HW_REGS_PER_PAGE) {
            buf[i] = m->pages[m->current_page][idx];
        } else {
            buf[i] = 0;
        }
    }
    return true;
}

#endif /* HW_MODEL_IS31FL3731_H */
