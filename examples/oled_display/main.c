// SPDX-License-Identifier: MIT
/*
 * oled_display -- Show BMP280 readings on SSD1306 OLED with printf_tiny
 *
 * Demonstrates:
 *   - SSD1306 OLED framebuffer display driver
 *   - BMP280 sensor data rendered as formatted text
 *   - printf_tiny for heap-free string formatting
 *   - Two I2C devices sharing the same bus (different addresses)
 *   - font_5x7 bitmap text rendering into framebuffer
 *
 * Wheels used:
 *   L0  printf_tiny   -- minimal printf (no heap, no float)
 *   L0  font_5x7      -- 5x7 bitmap font for display text
 *   L1  oled_ssd1306  -- SSD1306 128x64 OLED driver
 *   L1  bmp280        -- temperature + pressure sensor
 *
 * To adapt: replace HAL stubs with your MCU's I2C peripheral calls.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- Wheel headers --- */
#include "../../wheels/primitives/printf_tiny/printf_tiny.h"
#include "../../wheels/primitives/font_5x7/font_5x7.h"
#include "../../wheels/drivers/oled_ssd1306/oled_ssd1306.h"
#include "../../wheels/drivers/bmp280/bmp280.h"

/* ===================================================================
 * HAL Implementation (adapt for your MCU)
 * =================================================================== */

/* Both BMP280 (0x76) and SSD1306 (0x3C) share I2C1 */

static bool hal_bmp_write_reg(void *ctx, uint8_t addr, uint8_t reg,
                              uint8_t val)
{
    (void)ctx; (void)addr; (void)reg; (void)val;
    /* TODO: HAL_I2C_Mem_Write(&hi2c1, addr<<1, reg, 1, &val, 1, 100) */
    return true;
}

static bool hal_bmp_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                              uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr; (void)reg; (void)buf; (void)len;
    /* TODO: HAL_I2C_Mem_Read(&hi2c1, addr<<1, reg, 1, buf, len, 100) */
    return true;
}

static bool hal_oled_write_cmd(void *ctx, uint8_t addr, uint8_t cmd)
{
    (void)ctx; (void)addr; (void)cmd;
    /* TODO: uint8_t buf[2] = {0x00, cmd};
     *       HAL_I2C_Master_Transmit(&hi2c1, addr<<1, buf, 2, 100) */
    return true;
}

static bool hal_oled_write_data(void *ctx, uint8_t addr,
                                const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr; (void)data; (void)len;
    /* TODO: prepend 0x40 control byte, then transmit data block */
    return true;
}

static void hal_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    /* TODO: HAL_Delay(ms) */
}

/* ===================================================================
 * HAL binding tables
 * =================================================================== */

static const bmp280_i2c_t bmp_i2c = {
    .write_reg = hal_bmp_write_reg,
    .read_regs = hal_bmp_read_regs,
    .delay_ms  = hal_delay_ms,
    .ctx       = NULL,
};

static const oled_ssd1306_i2c_t oled_i2c = {
    .write_cmd  = hal_oled_write_cmd,
    .write_data = hal_oled_write_data,
    .ctx        = NULL,
};

/* ===================================================================
 * Display helpers -- render 5x7 text into SSD1306 framebuffer
 * =================================================================== */

static void draw_text(oled_ssd1306_t *oled, uint8_t x, uint8_t y,
                      const char *text)
{
    const int spacing = 1; /* 1-pixel gap between characters */
    uint8_t cx = x;

    for (const char *p = text; *p != '\0'; p++) {
        /* Render each glyph pixel-by-pixel from font_5x7 */
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                int pixel = font_5x7_pixel(&font_5x7_default, *p,
                                           col, row);
                if (pixel) {
                    oled_ssd1306_set_pixel(oled,
                                           (uint8_t)(cx + col),
                                           (uint8_t)(y + row), true);
                }
            }
        }
        cx = (uint8_t)(cx + 5 + spacing);
    }
}

/* ===================================================================
 * Application
 * =================================================================== */

static bmp280_t        bmp_dev;
static oled_ssd1306_t  oled_dev;
static uint8_t         framebuf[OLED_SSD1306_BUF_SIZE(128, 64)];

int main(void)
{
    /* 1. Initialize BMP280 sensor */
    bmp280_cfg_t bmp_cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_2,
        .osrs_p = BMP280_OVERSAMPLING_16,
    };
    if (bmp280_init(&bmp_dev, &bmp_i2c, &bmp_cfg) != BMP280_OK) {
        return 1;
    }

    /* 2. Initialize OLED display */
    oled_ssd1306_cfg_t oled_cfg = {
        .addr   = OLED_SSD1306_ADDR_A, /* 0x3C */
        .width  = 128,
        .height = 64,
    };
    if (oled_ssd1306_init(&oled_dev, &oled_i2c, &oled_cfg,
                          framebuf) != OLED_SSD1306_OK) {
        return 1;
    }
    oled_ssd1306_display_on(&oled_dev);

    /* 3. Display loop -- read sensor, format, render */
    for (;;) {
        bmp280_data_t data;
        if (bmp280_read(&bmp_dev, &data) != BMP280_OK) {
            hal_delay_ms(NULL, 500);
            continue;
        }

        /* Format readings with printf_tiny (no heap, no float) */
        int32_t temp_x10 = (int32_t)(data.temperature_c * 10.0f);
        int32_t press_hpa = (int32_t)(data.pressure_pa / 100.0f);

        char line1[22];
        char line2[22];
        snprintf_tiny(line1, sizeof(line1), "Temp: %ld.%ld C",
                      (long)(temp_x10 / 10), (long)(temp_x10 % 10));
        snprintf_tiny(line2, sizeof(line2), "Press: %ld hPa",
                      (long)press_hpa);

        /* Render to framebuffer */
        oled_ssd1306_clear(&oled_dev);
        draw_text(&oled_dev, 0,  0, "=== Weather ===");
        draw_text(&oled_dev, 0, 16, line1);
        draw_text(&oled_dev, 0, 32, line2);

        /* Flush framebuffer to display over I2C */
        oled_ssd1306_flush(&oled_dev);

        hal_delay_ms(NULL, 1000);
    }

    return 0;
}
