// SPDX-License-Identifier: MIT
/*
 * tca9548a — TCA9548A 8-channel I2C multiplexer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * TCA9548A has a single control register: each bit enables one of 8
 * downstream I2C channels.  Write one byte after the address to set
 * the channel mask; read one byte to query it.
 *
 * Uses the register-based I2C HAL (write_reg / read_regs) with
 * register address 0x00 — the chip treats any byte after its address
 * as the control value, so reg=0x00 is a harmless dummy.
 *
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef TCA9548A_H
#define TCA9548A_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TCA9548A_OK         =  0,
    TCA9548A_ERR_I2C    = -1,
    TCA9548A_ERR_PARAM = -2,
} tca9548a_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} tca9548a_i2c_t;

/** Valid 7-bit addresses: 0x70 .. 0x77 (A0-A2 pins). */
#define TCA9548A_ADDR_BASE  0x70u
#define TCA9548A_ADDR_MAX   0x77u
#define TCA9548A_MAX_CHANNEL 7u

typedef struct {
    const tca9548a_i2c_t *i2c;
    uint8_t dev_addr;          /* 7-bit (0x70 .. 0x77) */
    uint8_t current_channel;   /* cached mask for optimization */
} tca9548a_t;

/** Initialize device handle. @thread_safe: no @isr_safe: no */
tca9548a_err_t tca9548a_init(tca9548a_t *dev,
                             const tca9548a_i2c_t *i2c,
                             uint8_t dev_addr);

/** Select a single channel (0-7). @thread_safe: no @isr_safe: no */
tca9548a_err_t tca9548a_select(tca9548a_t *dev, uint8_t channel);

/** Select multiple channels via bitmask. @thread_safe: no @isr_safe: no */
tca9548a_err_t tca9548a_select_multi(tca9548a_t *dev, uint8_t mask);

/** Disable all channels (write 0x00). @thread_safe: no @isr_safe: no */
tca9548a_err_t tca9548a_disable_all(tca9548a_t *dev);

/** Read the current channel mask from the device. @thread_safe: no @isr_safe: no */
tca9548a_err_t tca9548a_get_channel(tca9548a_t *dev, uint8_t *mask);

#ifdef __cplusplus
}
#endif

#endif /* TCA9548A_H */
