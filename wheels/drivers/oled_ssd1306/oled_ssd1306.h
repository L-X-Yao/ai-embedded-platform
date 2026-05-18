// SPDX-License-Identifier: MIT
/*
 * oled_ssd1306 — SSD1306 128x64 OLED display driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Framebuffer-based: caller draws into a 128x64 buffer (1024 bytes),
 * then flushes to display. Includes basic pixel, line, and text drawing.
 * Pure C99, no heap. Framebuffer is caller-provided.
 */

#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OLED_SSD1306_OK        =  0,
    OLED_SSD1306_ERR_I2C   = -1,
    OLED_SSD1306_ERR_PARAM = -2,
} oled_ssd1306_err_t;

typedef struct {
    bool (*write_cmd)(void *ctx, uint8_t dev_addr, uint8_t cmd);
    bool (*write_data)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    void *ctx;
} oled_ssd1306_i2c_t;

typedef struct {
    uint8_t addr;
    uint8_t width;
    uint8_t height;
} oled_ssd1306_cfg_t;

#define OLED_SSD1306_BUF_SIZE(w, h)  ((w) * ((h) / 8))

typedef struct {
    const oled_ssd1306_i2c_t *i2c;
    oled_ssd1306_cfg_t cfg;
    uint8_t           *buf;
    size_t             buf_size;
} oled_ssd1306_t;

#define OLED_SSD1306_ADDR_A  0x3C
#define OLED_SSD1306_ADDR_B  0x3D

/** @thread_safe: no @isr_safe: no */
oled_ssd1306_err_t oled_ssd1306_init(oled_ssd1306_t *dev,
                                     const oled_ssd1306_i2c_t *i2c,
                                     const oled_ssd1306_cfg_t *cfg,
                                     uint8_t *framebuf);

/** @thread_safe: no @isr_safe: no */
oled_ssd1306_err_t oled_ssd1306_flush(oled_ssd1306_t *dev);

/** @thread_safe: no @isr_safe: yes */
void oled_ssd1306_clear(oled_ssd1306_t *dev);

/** @thread_safe: no @isr_safe: yes */
void oled_ssd1306_set_pixel(oled_ssd1306_t *dev, uint8_t x, uint8_t y, bool on);

/** @thread_safe: no @isr_safe: yes */
bool oled_ssd1306_get_pixel(const oled_ssd1306_t *dev, uint8_t x, uint8_t y);

/** @thread_safe: no @isr_safe: yes */
void oled_ssd1306_fill(oled_ssd1306_t *dev, bool on);

/** Turn display on (0xAF). @thread_safe: no @isr_safe: no */
oled_ssd1306_err_t oled_ssd1306_display_on(const oled_ssd1306_t *dev);

/** Turn display off / enter sleep (0xAE). @thread_safe: no @isr_safe: no */
oled_ssd1306_err_t oled_ssd1306_display_off(const oled_ssd1306_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* OLED_SSD1306_H */
