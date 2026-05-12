// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp3008.h -- MCP3008 SPI protocol model for testing
 *
 * Models the MCP3008 3-byte SPI transfer protocol:
 *   TX: [0x01] [(SGL/DIFF | CH) << 4] [0x00]
 *   RX: [x]    [0x00 | (raw >> 8)]    [raw & 0xFF]
 *
 * The model stores a raw value per channel (8 single-ended + 4 diff)
 * and responds to transfer() calls with the appropriate rx bytes.
 */

#ifndef HW_MODEL_MCP3008_H
#define HW_MODEL_MCP3008_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint16_t single[8];     /* raw values for single-ended channels 0-7 */
    uint16_t diff[4];       /* raw values for differential pairs 0-3 */
    bool     cs_active;
    bool     fail_transfer; /* simulate SPI failure */
    bool     fail_cs;       /* simulate CS select failure */
    int      transfer_count;
} mcp3008_model_t;

static void mcp3008_model_init(mcp3008_model_t *m)
{
    memset(m, 0, sizeof(*m));
}

static void mcp3008_model_set_single(mcp3008_model_t *m, uint8_t ch,
                                     uint16_t raw)
{
    if (ch < 8)
    {
        m->single[ch] = raw & 0x03FFu;
    }
}

static void mcp3008_model_set_diff(mcp3008_model_t *m, uint8_t pair,
                                   uint16_t raw)
{
    if (pair < 4)
    {
        m->diff[pair] = raw & 0x03FFu;
    }
}

static bool mcp3008_model_cs_select(void *ctx)
{
    mcp3008_model_t *m = (mcp3008_model_t *)ctx;
    if (m->fail_cs)
    {
        return false;
    }
    m->cs_active = true;
    return true;
}

static void mcp3008_model_cs_deselect(void *ctx)
{
    mcp3008_model_t *m = (mcp3008_model_t *)ctx;
    m->cs_active = false;
}

static bool mcp3008_model_transfer(void *ctx, const uint8_t *tx,
                                   uint8_t *rx, uint16_t len)
{
    mcp3008_model_t *m = (mcp3008_model_t *)ctx;
    m->transfer_count++;

    if (m->fail_transfer)
    {
        return false;
    }

    if (len != 3 || tx[0] != 0x01)
    {
        /* Unexpected protocol -- fill rx with 0 */
        memset(rx, 0, len);
        return true;
    }

    /* Decode config byte */
    uint8_t config = tx[1];
    bool single_ended = (config & 0x80u) != 0;
    uint8_t channel = (uint8_t)((config >> 4) & 0x07u);

    uint16_t raw;
    if (single_ended)
    {
        raw = (channel < 8) ? m->single[channel] : 0;
    }
    else
    {
        raw = (channel < 4) ? m->diff[channel] : 0;
    }

    rx[0] = 0x00;
    rx[1] = (uint8_t)((raw >> 8) & 0x03u);
    rx[2] = (uint8_t)(raw & 0xFFu);

    return true;
}

#endif /* HW_MODEL_MCP3008_H */
