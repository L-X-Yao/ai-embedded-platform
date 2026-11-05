// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp4728.h — MCP4728 hardware model from datasheet
 *
 * Models Microchip DS20002278 behavior:
 * - 4 channels (A-D), each 12-bit DAC output
 * - Multi-Write command: 0 1 0 0 0 DAC1 DAC0 UDAC (DS22187E Figure 5-8)
 * - Command byte passed as "reg" parameter to write_regs
 * - Data: [VREF PD1 PD0 Gx D11:D8] [D7:D0]
 * - Bytes 2-4 repeat for more channels; a repeated command byte is
 *   the next byte in the buffer, and its top five bits are don't care
 * - VREF, PD and Gx are kept per channel, VREF starting at the factory
 *   value of 1 (internal 2.048 V, Table 4-2)
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
    uint8_t  channel_vref[MCP4728_MODEL_NUM_CH];  /* 0 = VDD, 1 = internal */
    uint8_t  channel_pd[MCP4728_MODEL_NUM_CH];    /* PD1 PD0 */
    uint8_t  channel_gain[MCP4728_MODEL_NUM_CH];  /* 0 = x1, 1 = x2 */
    int      write_count;
    int      read_count;
    bool     fail_next;
} mcp4728_model_t;

static void mcp4728_model_init(mcp4728_model_t *m)
{
    memset(m, 0, sizeof(*m));
    memset(m->channel_vref, 1, sizeof(m->channel_vref));
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

    /* Multi-Write only: C2 C1 C0 W1 W0 = 0 1 0 0 0 (Table 5-1) */
    if ((reg & 0xF8u) != 0x40u) return false;

    /* 2 data bytes for the first channel, then 3 per repeated channel */
    if (len < 2 || (len - 2u) % 3u != 0) return false;

    uint8_t  cmd = reg;
    uint16_t i   = 0;
    for (;;) {
        uint8_t channel = (uint8_t)((cmd >> 1) & 0x03u);

        m->channel_vref[channel] = (uint8_t)(buf[i] >> 7);
        m->channel_pd[channel]   = (uint8_t)((buf[i] >> 5) & 0x03u);
        m->channel_gain[channel] = (uint8_t)((buf[i] >> 4) & 0x01u);
        m->channel_value[channel] =
            (uint16_t)(((uint16_t)(buf[i] & 0x0Fu) << 8) |
                       (uint16_t)buf[i + 1u]);

        if ((uint16_t)(i + 2u) >= len) break;
        cmd = buf[i + 2u];
        i   = (uint16_t)(i + 3u);
    }

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
