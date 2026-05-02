// SPDX-License-Identifier: MIT
/*
 * mcp23017 — I2C 16-bit GPIO expander driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * MCP23017: two 8-bit ports (A/B), configurable direction, pullups,
 * and interrupt-on-change. Bank=0 register addressing.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MCP23017_H
#define MCP23017_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCP23017_OK         =  0,
    MCP23017_ERR_I2C    = -1,
    MCP23017_ERR_PARAM = -2,
} mcp23017_err_t;

typedef enum {
    MCP23017_PORT_A = 0,
    MCP23017_PORT_B = 1,
} mcp23017_port_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} mcp23017_i2c_t;

typedef struct {
    const mcp23017_i2c_t *i2c;
    uint8_t dev_addr;           /* 7-bit (0x20-0x27, A0-A2 pins) */
} mcp23017_t;

/** Initialize device and reset all registers to defaults.
 *  dev_addr must be in range 0x20..0x27. @thread_safe: no */
mcp23017_err_t mcp23017_init(mcp23017_t *dev, const mcp23017_i2c_t *i2c,
                             uint8_t dev_addr);

/** Set I/O direction for a port. 1=input, 0=output. @thread_safe: no */
mcp23017_err_t mcp23017_set_direction(mcp23017_t *dev, mcp23017_port_t port,
                                      uint8_t mask);

/** Enable/disable internal 100k pullups. 1=enabled. @thread_safe: no */
mcp23017_err_t mcp23017_set_pullup(mcp23017_t *dev, mcp23017_port_t port,
                                   uint8_t mask);

/** Write all 8 pins of a port at once. @thread_safe: no */
mcp23017_err_t mcp23017_write_port(mcp23017_t *dev, mcp23017_port_t port,
                                   uint8_t value);

/** Read all 8 pins of a port at once. @thread_safe: no */
mcp23017_err_t mcp23017_read_port(mcp23017_t *dev, mcp23017_port_t port,
                                  uint8_t *value);

/** Write a single pin (0-7) via read-modify-write on OLAT. @thread_safe: no */
mcp23017_err_t mcp23017_write_pin(mcp23017_t *dev, mcp23017_port_t port,
                                  uint8_t pin, bool level);

/** Read a single pin (0-7). @thread_safe: no */
mcp23017_err_t mcp23017_read_pin(mcp23017_t *dev, mcp23017_port_t port,
                                 uint8_t pin, bool *level);

/** Set interrupt-on-change enable mask (GPINTEN). @thread_safe: no */
mcp23017_err_t mcp23017_set_interrupt(mcp23017_t *dev, mcp23017_port_t port,
                                      uint8_t mask);

/** Read interrupt flag register (INTF). @thread_safe: no */
mcp23017_err_t mcp23017_read_interrupt_flag(mcp23017_t *dev,
                                            mcp23017_port_t port,
                                            uint8_t *flags);

#ifdef __cplusplus
}
#endif

#endif /* MCP23017_H */
