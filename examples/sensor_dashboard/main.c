// SPDX-License-Identifier: MIT
/*
 * sensor_dashboard -- Read BMP280 + SHT30, log with structured output
 *
 * Demonstrates:
 *   - I2C HAL injection for two sensors sharing the same bus
 *   - Periodic sensor polling loop
 *   - Leveled, module-tagged logging via the log wheel
 *
 * Wheels used:
 *   L0  log      -- leveled logging with compile-time filtering
 *   L1  bmp280   -- temperature + pressure sensor (I2C)
 *   L1  sht30    -- temperature + humidity sensor (I2C)
 *
 * To adapt for your MCU: replace the HAL stubs in the
 * "HAL Implementation" section with real peripheral calls.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- Wheel headers --- */
#define LOG_MODULE "dashboard"
#include "../../wheels/primitives/log/log.h"
#include "../../wheels/drivers/bmp280/bmp280.h"
#include "../../wheels/drivers/sht30/sht30.h"

/* ===================================================================
 * HAL Implementation (adapt for your MCU)
 *
 * On STM32 you would call HAL_I2C_Mem_Write / HAL_I2C_Mem_Read.
 * On GD32 you would use i2c_master_write / i2c_master_read.
 * =================================================================== */

/* BMP280 uses register-based I2C (write_reg / read_regs) */

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

static void hal_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    /* TODO: HAL_Delay(ms) or vTaskDelay(pdMS_TO_TICKS(ms)) */
}

/* SHT30 uses raw I2C (write / read byte arrays) */

static bool hal_sht_write(void *ctx, uint8_t addr, const uint8_t *data,
                          uint16_t len)
{
    (void)ctx; (void)addr; (void)data; (void)len;
    /* TODO: HAL_I2C_Master_Transmit(&hi2c1, addr<<1, data, len, 100) */
    return true;
}

static bool hal_sht_read(void *ctx, uint8_t addr, uint8_t *data,
                         uint16_t len)
{
    (void)ctx; (void)addr; (void)data; (void)len;
    /* TODO: HAL_I2C_Master_Receive(&hi2c1, addr<<1, data, len, 100) */
    return true;
}

static void hal_sht_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
    /* TODO: HAL_Delay(ms) */
}

/* Log sink: routes log output to UART */

static void uart_log_sink(void *ctx, const char *line, size_t len)
{
    (void)ctx; (void)line; (void)len;
    /* TODO: HAL_UART_Transmit(&huart1, (uint8_t *)line, len, 100) */
}

/* Timestamp source for log */

static uint32_t hal_get_tick_ms(void)
{
    return 0; /* TODO: HAL_GetTick() or xTaskGetTickCount() */
}

/* ===================================================================
 * HAL binding tables -- connect wheels to hardware
 * =================================================================== */

static const bmp280_i2c_t bmp_i2c = {
    .write_reg = hal_bmp_write_reg,
    .read_regs = hal_bmp_read_regs,
    .delay_ms  = hal_delay_ms,
    .ctx       = NULL, /* pass &hi2c1 on real hardware */
};

static const sht30_i2c_t sht_i2c = {
    .write    = hal_sht_write,
    .read     = hal_sht_read,
    .delay_ms = hal_sht_delay,
    .ctx      = NULL,
};

/* ===================================================================
 * Application
 * =================================================================== */

static bmp280_t bmp_dev;
static sht30_t  sht_dev;

int main(void)
{
    /* 1. Initialize logging */
    log_set_level(LOG_LEVEL_DEBUG);
    log_add_sink(uart_log_sink, NULL);
    log_set_time_fn(hal_get_tick_ms);
    LOG_INFO("sensor dashboard starting");

    /* 2. Initialize BMP280 */
    bmp280_cfg_t bmp_cfg = {
        .addr   = BMP280_ADDR_LOW,   /* 0x76 -- SDO to GND */
        .osrs_t = BMP280_OVERSAMPLING_2,
        .osrs_p = BMP280_OVERSAMPLING_16,
    };
    if (bmp280_init(&bmp_dev, &bmp_i2c, &bmp_cfg) != BMP280_OK) {
        LOG_ERROR("BMP280 init failed");
    }

    /* 3. Initialize SHT30 */
    sht30_cfg_t sht_cfg = { .addr = SHT30_ADDR_A }; /* 0x44 */
    if (sht30_init(&sht_dev, &sht_i2c, &sht_cfg) != SHT30_OK) {
        LOG_ERROR("SHT30 init failed");
    }

    LOG_INFO("sensors initialized, entering poll loop");

    /* 4. Poll loop */
    for (;;) {
        bmp280_data_t bmp_data;
        if (bmp280_read(&bmp_dev, &bmp_data) == BMP280_OK) {
            LOG_DEBUG("BMP280: temp=%.1f C, press=%.0f Pa",
                      (double)bmp_data.temperature_c,
                      (double)bmp_data.pressure_pa);
        } else {
            LOG_WARN("BMP280 read failed");
        }

        sht30_data_t sht_data;
        if (sht30_read(&sht_dev, &sht_data) == SHT30_OK) {
            LOG_DEBUG("SHT30: temp=%.1f C, humid=%.1f %%",
                      (double)sht_data.temperature_c,
                      (double)sht_data.humidity_pct);
        } else {
            LOG_WARN("SHT30 read failed");
        }

        hal_delay_ms(NULL, 1000); /* 1 Hz polling */
    }

    return 0; /* unreachable */
}
