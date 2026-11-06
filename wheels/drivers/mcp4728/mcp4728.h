// SPDX-License-Identifier: MIT
/*
 * mcp4728 — Microchip MCP4728 4-channel 12-bit DAC
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef MCP4728_H
#define MCP4728_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MCP4728_OK        =  0,
    MCP4728_ERR_I2C   = -1,
    MCP4728_ERR_PARAM = -2,
} mcp4728_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} mcp4728_i2c_t;

/* ---------- constants ---------- */

#define MCP4728_ADDR_DEFAULT  0x60u
#define MCP4728_MAX_VALUE     4095u
#define MCP4728_NUM_CHANNELS  4u

/* ---------- configuration ---------- */

/* In vref_internal and gain_x2, bit n is channel n (bit 0 = A, bit 3 = D), and
 * 0 means VDD and gain x1. Init refuses gain x2 on a channel using VDD, because
 * the device then ignores it, and refuses any bit above bit 3. */
typedef struct {
    uint8_t addr;     /* I2C device address (default 0x60) */
    uint8_t vref_internal;   /* 1 = internal 2.048 V reference */
    uint8_t gain_x2;         /* 1 = gain x2 */
} mcp4728_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const mcp4728_i2c_t *i2c;
    uint8_t addr;
    uint8_t vref_internal;   /* copied from cfg, bit n is channel n */
    uint8_t gain_x2;         /* copied from cfg, bit n is channel n */
} mcp4728_t;

/* ---------- API ---------- */

/** Store configuration. Non-blocking (no I2C traffic).
 *  Returns MCP4728_ERR_PARAM for gain x2 on a channel using VDD.
 *  @thread_safe: no @isr_safe: no */
mcp4728_err_t mcp4728_init(mcp4728_t *dev, const mcp4728_i2c_t *i2c,
                           const mcp4728_cfg_t *cfg);

/** Write a single channel (0-3). Values > 4095 are rejected.
 *  Uses the MCP4728 Multi-Write command (0x40 | channel<<1), which also
 *  sets the channel's reference and gain from the configuration and puts it
 *  in normal mode. With both bits 0 that is VDD and gain x1, so a channel at
 *  the factory default leaves the internal 2.048 V reference at its first
 *  write and its output scale changes. Set vref_internal to keep it.
 *  @thread_safe: caller_must_lock @isr_safe: no */
mcp4728_err_t mcp4728_write_channel(mcp4728_t *dev, uint8_t channel,
                                    uint16_t value);

/** Write all 4 channels in one I2C transaction.
 *  Uses one Multi-Write with bytes 2-4 repeated per channel, with the same
 *  side effects as mcp4728_write_channel. Each output changes after its own
 *  last byte is acknowledged, so the four do not change at the same instant,
 *  and a bus error part way leaves the earlier channels updated.
 *  write_regs must accept 11 data bytes. Values > 4095 are rejected.
 *  @thread_safe: caller_must_lock @isr_safe: no */
mcp4728_err_t mcp4728_write_all(mcp4728_t *dev, const uint16_t values[4]);

#ifdef __cplusplus
}
#endif

#endif /* MCP4728_H */
