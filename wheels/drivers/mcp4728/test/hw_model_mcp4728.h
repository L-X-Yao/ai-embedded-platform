// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp4728.h — MCP4728 hardware model from datasheet
 *
 * Models Microchip DS20002278 behavior:
 * - 4 channels (A-D), each 12-bit DAC output
 * - Fast write command: 0x40 | (channel << 1)
 * - Command byte passed as "reg" parameter to write_regs
 * - Data: 2 bytes [D11:D8 in low nibble of byte 0] [D7:D0]
 *
 * Validates command byte encoding against MCP4728 protocol.
 */

#ifndef HW_MODEL_MCP4728_H
#define HW_MODEL_MCP4728_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MCP4728_MODEL_NUM_CH  4

typedef struct {
    uint16_t channel_value[MCP4728_MODEL_NUM_CH];
    int      write_count;
    int      read_count;
    bool     fail_next;
} mcp4728_model_t;

static void mcp4728_model_init(mcp4728_model_t *m)
{
    memset(m, 0, sizeof(*m));
}

__attribute__((unused))
static bool mcp4728_model_write_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, const uint8_t *buf,
                                     uint16_t len)
{
    (void)dev_addr;
    mcp4728_model_t *m = (mcp4728_model_t *)ctx;

    if (m->fail_next) {
        m->fail_next = false;
        return false;
    }

    m->write_count++;

    /* Validate fast write command format: 010 0 CC 0 */
    if ((reg & 0xC0u) != 0x40u) return false;  /* not a fast write */
    if (len != 2) return false;

    uint8_t channel = (uint8_t)((reg >> 1) & 0x03u);
    if (channel >= MCP4728_MODEL_NUM_CH) return false;

    /* Upper nibble of buf[0] must be zero for fast write */
    if ((buf[0] & 0xF0u) != 0) return false;

    uint16_t value = (uint16_t)(((uint16_t)(buf[0] & 0x0Fu) << 8) |
                                (uint16_t)buf[1]);
    m->channel_value[channel] = value;

    return true;
}

__attribute__((unused))
static bool mcp4728_model_read_regs(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t *buf,
                                    uint16_t len)
{
    (void)dev_addr; (void)reg;
    mcp4728_model_t *m = (mcp4728_model_t *)ctx;
    m->read_count++;

    /* Zero-fill; MCP4728 read-back is not used by current API */
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = 0;
    }
    return true;
}

#endif /* HW_MODEL_MCP4728_H */
