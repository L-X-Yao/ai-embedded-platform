// SPDX-License-Identifier: MIT
/*
 * mcp4725 — MCP4725 12-bit I2C DAC driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected, raw write/read)
 * Instances: multi
 *
 * Supports fast write (volatile DAC), DAC register write, and
 * EEPROM write for persistent output. Power-down modes supported.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MCP4725_H
#define MCP4725_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCP4725_OK        =  0,
    MCP4725_ERR_I2C   = -1,
    MCP4725_ERR_PARAM = -2,
    MCP4725_ERR_BUSY  = -3,   /* EEPROM write running, nothing was sent */
} mcp4725_err_t;

typedef enum {
    MCP4725_PD_NORMAL = 0,
    MCP4725_PD_1K     = 1,
    MCP4725_PD_100K   = 2,
    MCP4725_PD_500K   = 3,
} mcp4725_pd_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void *ctx;
} mcp4725_i2c_t;

typedef struct {
    uint16_t    dac_value;
    uint8_t     pd_mode;
    bool        eeprom_busy;
    uint16_t    eeprom_value;
    uint8_t     eeprom_pd;
} mcp4725_status_t;

typedef struct {
    const mcp4725_i2c_t *i2c;
    uint8_t addr;
    bool    eeprom_pending;   /* private: RDY/BSY not yet seen high */
} mcp4725_t;

#define MCP4725_ADDR_A  0x60u
#define MCP4725_ADDR_B  0x61u

#define MCP4725_MAX_VALUE  4095u

/** Verify communication by reading back status. @thread_safe: no @isr_safe: no */
mcp4725_err_t mcp4725_init(mcp4725_t *dev, const mcp4725_i2c_t *i2c,
                           uint8_t addr);

/** Fast-mode write: update DAC output (volatile, no EEPROM). @thread_safe: no @isr_safe: no */
mcp4725_err_t mcp4725_write_fast(mcp4725_t *dev, uint16_t value);

/** Write DAC register with power-down control. @thread_safe: no @isr_safe: no */
mcp4725_err_t mcp4725_write_dac(mcp4725_t *dev, uint16_t value,
                                mcp4725_pd_t pd);

/**
 * Write DAC register and EEPROM (persistent across power cycle).
 * Returns at once. The EEPROM cycle takes 25 to 50 ms, and until it ends
 * every write function returns MCP4725_ERR_BUSY.
 * @thread_safe: no @isr_safe: no
 */
mcp4725_err_t mcp4725_write_eeprom(mcp4725_t *dev, uint16_t value,
                                   mcp4725_pd_t pd);

/** Read back DAC register, EEPROM, and busy status. @thread_safe: no @isr_safe: no */
mcp4725_err_t mcp4725_read_status(mcp4725_t *dev, mcp4725_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MCP4725_H */
