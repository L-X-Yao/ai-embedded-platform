// SPDX-License-Identifier: MIT
/*
 * hw_model_max31865.h — MAX31865 hardware register model
 *
 * Models Maxim MAX31865 SPI register behavior:
 * - Config register 0x00 (r/w, write via 0x80)
 * - RTD MSB register 0x01, RTD LSB register 0x02 (read-only)
 * - Fault register 0x07 (read-only, clearable via config bit)
 * - 15-bit RTD ADC value: [15:1] = ADC, [0] = fault flag
 * - SPI protocol: read = addr, write = addr | 0x80
 *
 * The model uses SPI byte-at-a-time xfer to simulate register access.
 */

#ifndef HW_MODEL_MAX31865_H
#define HW_MODEL_MAX31865_H

#include "hw_model.h"

/* --- MAX31865 register addresses --- */
#define MAX_REG_CONFIG    0x00u
#define MAX_REG_RTD_MSB   0x01u
#define MAX_REG_RTD_LSB   0x02u
#define MAX_REG_HFAULT_H  0x03u
#define MAX_REG_HFAULT_L  0x04u
#define MAX_REG_LFAULT_H  0x05u
#define MAX_REG_LFAULT_L  0x06u
#define MAX_REG_FAULT     0x07u

#define MAX_WRITE_BIT     0x80u

/* Config register bits */
#define MAX_CFG_BIAS        0x80u
#define MAX_CFG_AUTO        0x40u
#define MAX_CFG_CLEAR_FAULT 0x02u
#define MAX_CFG_3WIRE       0x10u
#define MAX_CFG_50HZ        0x01u

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    uint16_t         true_rtd_raw;   /* 15-bit RTD ADC (0..32767) */
    uint8_t          fault_status;   /* fault register value */
    /* SPI transaction state */
    uint8_t          spi_addr;       /* current register address */
    bool             spi_writing;    /* write mode (addr had bit 7 set) */
    bool             spi_first;      /* next xfer is address byte */
    int              cs_state;       /* 0=high, 1=low */
} max31865_model_t;

/* Update RTD registers from true_rtd_raw */
static void max31865_model_update_rtd(max31865_model_t *m)
{
    /* RTD value in bits [15:1], fault flag in bit [0] */
    uint16_t rtd_word = (uint16_t)(m->true_rtd_raw << 1);
    if (m->fault_status)
        rtd_word |= 0x01;  /* set fault flag */
    m->hw.regs[MAX_REG_RTD_MSB] = (uint8_t)(rtd_word >> 8);
    m->hw.regs[MAX_REG_RTD_LSB] = (uint8_t)(rtd_word & 0xFF);
}

/* Callback: config register write — handle fault clear */
static void max31865_model_on_config_write(void *user, uint8_t reg,
                                           uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max31865_model_t *m = (max31865_model_t *)user;
    if (new_val & MAX_CFG_CLEAR_FAULT) {
        m->fault_status = 0;
        m->hw.regs[MAX_REG_FAULT] = 0;
        max31865_model_update_rtd(m);
        /* Clear the fault-clear bit (it's self-clearing) */
        m->hw.regs[MAX_REG_CONFIG] &= (uint8_t)~MAX_CFG_CLEAR_FAULT;
    }
}

/* --- SPI HAL callbacks --- */

__attribute__((unused))
static void max31865_model_cs_low(void *ctx)
{
    max31865_model_t *m = (max31865_model_t *)ctx;
    m->cs_state = 1;
    m->spi_first = true;
}

__attribute__((unused))
static void max31865_model_cs_high(void *ctx)
{
    max31865_model_t *m = (max31865_model_t *)ctx;
    m->cs_state = 0;
}

__attribute__((unused))
static uint8_t max31865_model_xfer(void *ctx, uint8_t tx)
{
    max31865_model_t *m = (max31865_model_t *)ctx;

    if (m->spi_first) {
        /* First byte is address */
        m->spi_first = false;
        m->spi_writing = (tx & MAX_WRITE_BIT) != 0;
        m->spi_addr = tx & 0x7F;
        return 0xFF;
    }

    if (m->spi_writing) {
        /* Write to register */
        hw_model__write_reg(&m->hw, m->spi_addr, tx);
        m->spi_addr++;
        return 0xFF;
    } else {
        /* Read from register */
        uint8_t val = m->hw.regs[m->spi_addr];
        m->spi_addr++;
        return val;
    }
}

static void max31865_model_init(max31865_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Config register: writable, power-on reset = 0x00 */
    hw_model_set_on_write(&m->hw, MAX_REG_CONFIG, max31865_model_on_config_write);

    /* RTD registers: read-only (set by model) */
    hw_model_set_readonly(&m->hw, MAX_REG_RTD_MSB);
    hw_model_set_readonly(&m->hw, MAX_REG_RTD_LSB);

    /* Fault register: read-only */
    hw_model_set_readonly(&m->hw, MAX_REG_FAULT);

    /* Threshold registers: writable */
    /* (default 0x00, typical application doesn't use them) */

    /* Default RTD: PT100 at 25C, R = 109.73 ohm, with Rref=430
     * rtd_raw = R / Rref * 32768 = 109.73/430 * 32768 = 8360 */
    m->true_rtd_raw = 8360;
    m->fault_status = 0;

    hw_model_reset(&m->hw);
    max31865_model_update_rtd(m);
}

/* Helper: set simulated 15-bit RTD ADC value */
static void max31865_model_set_rtd(max31865_model_t *m, uint16_t rtd_raw)
{
    m->true_rtd_raw = rtd_raw & 0x7FFF;
    max31865_model_update_rtd(m);
}

/* Helper: inject a fault */
static void max31865_model_set_fault(max31865_model_t *m, uint8_t fault)
{
    m->fault_status = fault;
    m->hw.regs[MAX_REG_FAULT] = fault;
    max31865_model_update_rtd(m);
}

#endif /* HW_MODEL_MAX31865_H */
