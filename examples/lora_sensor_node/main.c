// SPDX-License-Identifier: MIT
/*
 * lora_sensor_node -- Read ADXL345 accelerometer, COBS-frame, TX via SX1262
 *
 * Demonstrates:
 *   - I2C HAL for the accelerometer
 *   - SPI HAL for the LoRa radio (separate bus)
 *   - COBS framing for reliable serial/wireless payloads
 *   - Two different HAL types (I2C + SPI) on the same MCU
 *
 * Wheels used:
 *   L0  cobs     -- byte-stuffing for zero-delimited framing
 *   L1  adxl345  -- 3-axis accelerometer driver (I2C)
 *   L1  sx1262   -- LoRa radio transceiver (SPI)
 *
 * To adapt: replace HAL stubs with your MCU's I2C/SPI peripheral calls.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* --- Wheel headers --- */
#include "../../wheels/drivers/adxl345/adxl345.h"
#include "../../wheels/drivers/sx1262/sx1262.h"
#include "../../wheels/primitives/cobs/cobs.h"

/* ===================================================================
 * HAL Implementation (adapt for your MCU)
 * =================================================================== */

/* --- I2C HAL for ADXL345 --- */

static bool hal_i2c_write_reg(void *ctx, uint8_t addr, uint8_t reg,
                              uint8_t val)
{
    (void)ctx; (void)addr; (void)reg; (void)val;
    /* TODO: I2C mem write -- e.g. HAL_I2C_Mem_Write() on STM32 */
    return true;
}

static bool hal_i2c_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                              uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr; (void)reg; (void)buf; (void)len;
    /* TODO: I2C mem read -- e.g. HAL_I2C_Mem_Read() on STM32 */
    return true;
}

static void hal_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    /* TODO: HAL_Delay(ms) */
}

/* --- SPI HAL for SX1262 --- */

static void hal_spi_cs_select(void *ctx)
{
    (void)ctx;
    /* TODO: HAL_GPIO_WritePin(SX_NSS_GPIO, SX_NSS_PIN, GPIO_PIN_RESET) */
}

static void hal_spi_cs_deselect(void *ctx)
{
    (void)ctx;
    /* TODO: HAL_GPIO_WritePin(SX_NSS_GPIO, SX_NSS_PIN, GPIO_PIN_SET) */
}

static void hal_spi_transfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                             size_t len)
{
    (void)ctx; (void)tx; (void)rx; (void)len;
    /* TODO: HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, 100) */
}

static bool hal_spi_is_busy(void *ctx)
{
    (void)ctx;
    /* TODO: return HAL_GPIO_ReadPin(SX_BUSY_GPIO, SX_BUSY_PIN) */
    return false;
}

/* ===================================================================
 * HAL binding tables
 * =================================================================== */

static const adxl345_i2c_t accel_i2c = {
    .write_reg = hal_i2c_write_reg,
    .read_regs = hal_i2c_read_regs,
    .delay_ms  = hal_delay_ms,
    .ctx       = NULL,
};

static const sx1262_hal_t lora_hal = {
    .cs_select    = hal_spi_cs_select,
    .cs_deselect  = hal_spi_cs_deselect,
    .transfer     = hal_spi_transfer,
    .is_busy      = hal_spi_is_busy,
    .delay_ms     = hal_delay_ms,
    .get_spi_error = NULL,
    .ctx          = NULL,
};

/* ===================================================================
 * Packet format: 12-byte payload (3x float), COBS-encoded
 *
 * Raw:  [x_g:4][y_g:4][z_g:4]  = 12 bytes
 * COBS: max 14 bytes encoded + 1 delimiter = 15 bytes on the wire
 * =================================================================== */

typedef struct {
    float x_g;
    float y_g;
    float z_g;
} accel_packet_t;

/* ===================================================================
 * Application
 * =================================================================== */

static adxl345_t accel_dev;
static sx1262_t  lora_dev;

int main(void)
{
    /* 1. Initialize accelerometer (I2C) */
    adxl345_cfg_t accel_cfg = {
        .addr      = ADXL345_ADDR_PRIMARY, /* 0x53 */
        .range     = ADXL345_RANGE_4G,
        .data_rate = ADXL345_RATE_100HZ,
    };
    if (adxl345_init(&accel_dev, &accel_i2c, &accel_cfg) != ADXL345_OK) {
        return 1; /* accelerometer not found */
    }

    /* 2. Initialize LoRa radio (SPI) -- 868 MHz, SF7, BW 125kHz */
    if (sx1262_init(&lora_dev, &lora_hal) != SX1262_OK) {
        return 1;
    }
    sx1262_set_frequency(&lora_dev, 868000000);
    sx1262_configure_lora(&lora_dev, SX1262_SF7, SX1262_BW_125K,
                          SX1262_CR_4_5);

    /* 3. Sense-and-send loop */
    for (;;) {
        /* Read accelerometer */
        adxl345_data_t accel_data;
        if (adxl345_read(&accel_dev, &accel_data) != ADXL345_OK) {
            hal_delay_ms(NULL, 100);
            continue;
        }

        /* Pack into struct (little-endian on Cortex-M) */
        accel_packet_t pkt;
        pkt.x_g = accel_data.x_g;
        pkt.y_g = accel_data.y_g;
        pkt.z_g = accel_data.z_g;

        /* COBS-encode for framing (eliminates 0x00 bytes) */
        uint8_t raw[sizeof(accel_packet_t)];
        memcpy(raw, &pkt, sizeof(raw));

        uint8_t encoded[16];
        uint16_t enc_len = sizeof(encoded);
        if (cobs_encode(raw, sizeof(raw), encoded, &enc_len) != COBS_OK) {
            continue;
        }

        /* Transmit over LoRa */
        sx1262_transmit(&lora_dev, encoded, (uint8_t)enc_len);

        /* Wait before next sample */
        hal_delay_ms(NULL, 1000);
    }

    return 0;
}
