// SPDX-License-Identifier: MIT
/*
 * sh1106 — SH1106 132x64 OLED display controller driver (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Framebuffer-based: caller provides a 128x64 buffer (1024 bytes),
 * draws into it, then flushes to the display. SH1106 uses page
 * addressing with a 2-column offset (columns 2-129 for 128px width).
 * Pure C99, no heap. Framebuffer is caller-provided.
 */

#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SH1106_OK        =  0,
    SH1106_ERR_I2C   = -1,
    SH1106_ERR_ID    = -2,
    SH1106_ERR_PARAM = -3,
} sh1106_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} sh1106_i2c_t;

/* ---------- constants ---------- */

#define SH1106_ADDR_A      0x3C
#define SH1106_ADDR_B      0x3D
#define SH1106_WIDTH       128u
#define SH1106_HEIGHT      64u
#define SH1106_BUF_SIZE    ((SH1106_WIDTH * SH1106_HEIGHT) / 8u)  /* 1024 */
#define SH1106_COL_OFFSET  2u     /* SH1106 has 132 columns; 128px starts at col 2 */
#define SH1106_PAGES       (SH1106_HEIGHT / 8u)

/* ---------- device context ---------- */

typedef struct {
    const sh1106_i2c_t *i2c;
    uint8_t             addr;
    uint8_t            *buf;
} sh1106_t;

/* ---------- API ---------- */

/** Initialize display and send startup commands.
 *  @param dev      Device context (caller-owned)
 *  @param i2c      I2C HAL callbacks
 *  @param addr     I2C address (SH1106_ADDR_A or SH1106_ADDR_B)
 *  @param framebuf Caller-provided buffer, must be SH1106_BUF_SIZE bytes
 *  @thread_safe: no @isr_safe: no */
sh1106_err_t sh1106_init(sh1106_t *dev, const sh1106_i2c_t *i2c,
                         uint8_t addr, uint8_t *framebuf);

/** Flush framebuffer to display (page-by-page). @thread_safe: no @isr_safe: no */
sh1106_err_t sh1106_display(sh1106_t *dev);

/** Clear framebuffer (set all pixels off). @thread_safe: no @isr_safe: yes */
void sh1106_clear(sh1106_t *dev);

/** Set a single pixel in the framebuffer. @thread_safe: no @isr_safe: yes */
void sh1106_set_pixel(sh1106_t *dev, uint8_t x, uint8_t y, bool on);

/** Get a single pixel from the framebuffer. @thread_safe: no @isr_safe: yes */
bool sh1106_get_pixel(const sh1106_t *dev, uint8_t x, uint8_t y);

/** Fill entire framebuffer with on or off. @thread_safe: no @isr_safe: yes */
void sh1106_fill(sh1106_t *dev, bool on);

/** Set display contrast (0x00-0xFF). @thread_safe: no @isr_safe: no */
sh1106_err_t sh1106_set_contrast(sh1106_t *dev, uint8_t val);

/** Turn display on (0xAF). @thread_safe: no @isr_safe: no */
sh1106_err_t sh1106_display_on(const sh1106_t *dev);

/** Turn display off / enter sleep (0xAE). @thread_safe: no @isr_safe: no */
sh1106_err_t sh1106_display_off(const sh1106_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SH1106_H */
