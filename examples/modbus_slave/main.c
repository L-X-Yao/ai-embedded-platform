// SPDX-License-Identifier: MIT
/*
 * modbus_slave -- Expose BMP280 sensor data as Modbus RTU registers
 *
 * Demonstrates:
 *   - Modbus RTU slave processing with register callbacks
 *   - Ring buffer for UART RX buffering
 *   - BMP280 sensor values mapped to Modbus input registers
 *   - UART-based protocol integration pattern
 *
 * Wheels used:
 *   L0  ring_buffer  -- UART RX byte buffering
 *   L1  bmp280       -- temperature + pressure sensor
 *   L2  modbus_rtu   -- Modbus RTU frame codec + slave handler
 *
 * Register map (input registers, FC 0x04):
 *   0x0000  temperature (raw, x100 -> 2512 = 25.12 C)
 *   0x0001  pressure    (raw, hPa integer -> 1013 = 1013 hPa)
 *
 * To adapt: replace HAL stubs, wire UART RX ISR to ring_buffer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* --- Wheel headers --- */
#include "../../wheels/primitives/ring_buffer/ring_buffer.h"
#include "../../wheels/drivers/bmp280/bmp280.h"
#include "../../wheels/protocols/modbus_rtu/modbus_rtu.h"

/* ===================================================================
 * HAL Implementation (adapt for your MCU)
 * =================================================================== */

/* --- I2C HAL for BMP280 --- */

static bool hal_i2c_write_reg(void *ctx, uint8_t addr, uint8_t reg,
                              uint8_t val)
{
    (void)ctx; (void)addr; (void)reg; (void)val;
    /* TODO: HAL_I2C_Mem_Write(&hi2c1, addr<<1, reg, 1, &val, 1, 100) */
    return true;
}

static bool hal_i2c_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                              uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr; (void)reg; (void)buf; (void)len;
    /* TODO: HAL_I2C_Mem_Read(&hi2c1, addr<<1, reg, 1, buf, len, 100) */
    return true;
}

static void hal_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    /* TODO: HAL_Delay(ms) */
}

/* --- UART HAL for Modbus --- */

static void hal_uart_send(const uint8_t *data, uint16_t len)
{
    (void)data; (void)len;
    /* TODO: HAL_UART_Transmit(&huart2, data, len, 100)
     * On RS-485: assert DE pin before TX, deassert after TX complete */
}

/* ===================================================================
 * HAL binding tables
 * =================================================================== */

static const bmp280_i2c_t bmp_i2c = {
    .write_reg = hal_i2c_write_reg,
    .read_regs = hal_i2c_read_regs,
    .delay_ms  = hal_delay_ms,
    .ctx       = NULL,
};

/* ===================================================================
 * Shared state -- latest sensor readings
 * =================================================================== */

static volatile int32_t g_temp_x100;   /* temperature * 100 */
static volatile int32_t g_press_hpa;   /* pressure in hPa   */

/* ===================================================================
 * Modbus register callbacks
 *
 * The slave handler calls these when a master requests registers.
 * Input registers (FC 0x04) are read-only sensor values.
 * =================================================================== */

static int modbus_read_input(void *ctx, uint16_t addr, uint16_t *value)
{
    (void)ctx;
    switch (addr) {
    case 0x0000: *value = (uint16_t)(g_temp_x100 & 0xFFFF); return 0;
    case 0x0001: *value = (uint16_t)(g_press_hpa & 0xFFFF); return 0;
    default:     return -1; /* address out of range */
    }
}

/* ===================================================================
 * UART RX buffering
 *
 * In a real system the UART RX ISR pushes bytes into the ring buffer.
 * The main loop drains it and checks for complete Modbus frames.
 * =================================================================== */

#define UART_RX_SIZE  256
static ring_buffer_t  uart_rx_rb;
static uint8_t        uart_rx_store[UART_RX_SIZE];

/*
 * Call this from your UART RX ISR:
 *
 *   void USART2_IRQHandler(void) {
 *       uint8_t byte = USART2->DR;
 *       ring_buffer_write(&uart_rx_rb, &byte, 1);
 *   }
 */

/* ===================================================================
 * Application
 * =================================================================== */

static bmp280_t          bmp_dev;
static modbus_rtu_slave_t mb_slave;

int main(void)
{
    /* 1. Initialize UART RX ring buffer */
    ring_buffer_init(&uart_rx_rb, uart_rx_store, UART_RX_SIZE);

    /* 2. Initialize BMP280 */
    bmp280_cfg_t bmp_cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_2,
        .osrs_p = BMP280_OVERSAMPLING_16,
    };
    if (bmp280_init(&bmp_dev, &bmp_i2c, &bmp_cfg) != BMP280_OK) {
        return 1;
    }

    /* 3. Initialize Modbus RTU slave (address 1) */
    modbus_rtu_slave_init(&mb_slave, 1, NULL);
    mb_slave.read_input = modbus_read_input;

    /* 4. Main loop: poll sensor + process Modbus requests */
    uint32_t poll_counter = 0;

    for (;;) {
        /* Update sensor readings every ~100 iterations */
        if (++poll_counter >= 100) {
            poll_counter = 0;
            bmp280_data_t data;
            if (bmp280_read(&bmp_dev, &data) == BMP280_OK) {
                g_temp_x100 = (int32_t)(data.temperature_c * 100.0f);
                g_press_hpa = (int32_t)(data.pressure_pa / 100.0f);
            }
        }

        /* Drain UART RX buffer and process Modbus frames */
        uint16_t available = ring_buffer_count(&uart_rx_rb);
        if (available >= MODBUS_RTU_MIN_FRAME) {
            uint8_t req[MODBUS_RTU_MAX_FRAME];
            uint16_t len = ring_buffer_read(&uart_rx_rb, req, available);

            uint8_t resp[MODBUS_RTU_MAX_FRAME];
            int resp_len = modbus_rtu_slave_process(
                &mb_slave, req, len, resp, sizeof(resp));

            if (resp_len > 0) {
                hal_uart_send(resp, (uint16_t)resp_len);
            }
        }
    }

    return 0;
}
