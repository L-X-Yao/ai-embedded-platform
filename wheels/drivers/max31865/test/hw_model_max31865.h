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
 * - POR values of p.13: RTD 0000h, High Fault Threshold FFFFh, Low 0000h
 * - Conversions run with BIAS and auto mode set. Each one compares the result
 *   with the thresholds (p.16) and latches D7 or D6 in the fault register
 *
 * Time passes only in max31865_model_advance_us(). A conversion ends after the
 * maximum tCONV of p.3: the single conversion time for the first one after auto
 * mode starts (p.4 note 5), then the continuous time. Light set with
 * max31865_model_set_rtd() reaches the registers at the end of a conversion.
 * 1-shot conversions and the fault detection cycle (D5, D3:D2) are not modelled.
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
#define MAX_CFG_NO_CLEAR    0x2Cu  /* D5, D3, D2: must be 0 for a fault clear */

/* Fault status bits */
#define MAX_FAULT_HIGH      0x80u
#define MAX_FAULT_LOW       0x40u

/* p.3: maximum tCONV in microseconds */
#define MAX_SINGLE_60HZ_US     55000u
#define MAX_SINGLE_50HZ_US     66000u
#define MAX_CONTINUOUS_60HZ_US 17600u
#define MAX_CONTINUOUS_50HZ_US 21000u

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    uint16_t         true_rtd_raw;   /* 15-bit RTD ADC (0..32767) */
    uint16_t         rtd_code;       /* result of the last conversion */
    uint8_t          fault_status;   /* fault register value */
    bool             converting;     /* BIAS and auto mode are on */
    uint32_t         until_conversion_us;
    /* SPI transaction state */
    uint8_t          spi_addr;       /* current register address */
    bool             spi_writing;    /* write mode (addr had bit 7 set) */
    bool             spi_first;      /* next xfer is address byte */
    int              cs_state;       /* 0=high, 1=low */
} max31865_model_t;

/* Update RTD registers from the last conversion */
static void max31865_model_update_rtd(max31865_model_t *m)
{
    /* RTD value in bits [15:1], fault flag in bit [0] */
    uint16_t rtd_word = (uint16_t)(m->rtd_code << 1);
    if (m->fault_status)
        rtd_word |= 0x01;  /* set fault flag */
    m->hw.regs[MAX_REG_RTD_MSB] = (uint8_t)(rtd_word >> 8);
    m->hw.regs[MAX_REG_RTD_LSB] = (uint8_t)(rtd_word & 0xFF);
}

/* A threshold pair in the 15-bit form of the RTD code (p.16: same format) */
static uint16_t max31865_model_threshold(const max31865_model_t *m, uint8_t reg)
{
    return (uint16_t)(((uint16_t)m->hw.regs[reg] << 8 |
                       m->hw.regs[(uint8_t)(reg + 1)]) >> 1);
}

/* A conversion ends. p.16: the RTD High bit is set at or above the high
 * threshold, the RTD Low bit at or below the low one, and both latch. */
static void max31865_model_convert(max31865_model_t *m)
{
    m->rtd_code = m->true_rtd_raw;
    if (m->rtd_code >= max31865_model_threshold(m, MAX_REG_HFAULT_H)) {
        m->fault_status |= MAX_FAULT_HIGH;
    }
    if (m->rtd_code <= max31865_model_threshold(m, MAX_REG_LFAULT_H)) {
        m->fault_status |= MAX_FAULT_LOW;
    }
    m->hw.regs[MAX_REG_FAULT] = m->fault_status;
    max31865_model_update_rtd(m);
}

/* Callback: config register write — start conversions, handle fault clear */
static void max31865_model_on_config_write(void *user, uint8_t reg,
                                           uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max31865_model_t *m = (max31865_model_t *)user;
    uint8_t both = MAX_CFG_BIAS | MAX_CFG_AUTO;
    bool now_converting = (new_val & both) == both;

    /* p.4 note 5: the first one takes the single conversion time */
    if (now_converting && !m->converting) {
        m->until_conversion_us = (new_val & MAX_CFG_50HZ) ? MAX_SINGLE_50HZ_US
                                                          : MAX_SINGLE_60HZ_US;
    }
    m->converting = now_converting;

    if (new_val & MAX_CFG_CLEAR_FAULT) {
        /* p.14: "Write a 1 to this bit while writing 0 to bits D5, D3, and D2" */
        if (!(new_val & MAX_CFG_NO_CLEAR)) {
            m->fault_status = 0;
            m->hw.regs[MAX_REG_FAULT] = 0;
            max31865_model_update_rtd(m);
        }
        /* Clear the fault-clear bit (it's self-clearing) */
        m->hw.regs[MAX_REG_CONFIG] &= (uint8_t)~MAX_CFG_CLEAR_FAULT;
    }
}

/* Let time pass: conversions end one after the other while they run */
__attribute__((unused))
static void max31865_model_advance_us(max31865_model_t *m, uint32_t us)
{
    if (!m->converting) return;

    while (us >= m->until_conversion_us) {
        us -= m->until_conversion_us;
        max31865_model_convert(m);
        m->until_conversion_us = (m->hw.regs[MAX_REG_CONFIG] & MAX_CFG_50HZ)
                               ? MAX_CONTINUOUS_50HZ_US
                               : MAX_CONTINUOUS_60HZ_US;
    }
    m->until_conversion_us -= us;
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

    /* Threshold registers: writable. p.13: the high pair starts at FFFFh. */
    hw_model_set_reset_value(&m->hw, MAX_REG_HFAULT_H, 0xFF);
    hw_model_set_reset_value(&m->hw, MAX_REG_HFAULT_L, 0xFF);

    /* Default RTD: PT100 at 25C, R = 109.73 ohm, with Rref=430
     * rtd_raw = R / Rref * 32768 = 109.73/430 * 32768 = 8360. p.13: the RTD
     * registers themselves stay at 00h until a conversion ends. */
    m->true_rtd_raw = 8360;
    m->fault_status = 0;

    hw_model_reset(&m->hw);
}

/* Helper: set the simulated 15-bit RTD ADC value of the next conversion */
__attribute__((unused))
static void max31865_model_set_rtd(max31865_model_t *m, uint16_t rtd_raw)
{
    m->true_rtd_raw = rtd_raw & 0x7FFF;
}

/* Helper: inject a fault */
__attribute__((unused))
static void max31865_model_set_fault(max31865_model_t *m, uint8_t fault)
{
    m->fault_status = fault;
    m->hw.regs[MAX_REG_FAULT] = fault;
    max31865_model_update_rtd(m);
}

#endif /* HW_MODEL_MAX31865_H */
