// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp3008.h -- MCP3008 SPI protocol model for testing
 *
 * Models the MCP3008 3-byte SPI transfer protocol:
 *   TX: [0x01] [(SGL/DIFF | CH) << 4] [0x00]
 *   RX: [?]    [? ? ? ? ? 0 B9 B8]    [B7 .. B0]
 *
 * The model stores a raw value per channel (8 single-ended + 4 diff)
 * and responds to transfer() calls with the appropriate rx bytes.
 *
 * From DS21295D: the "?" bits of Figure 6-1 (p21) read as unknown_bits, so a
 * dropped mask shows. The chip answers only while CS is low, and once
 * powered up with CS low only after CS has been high (p19). Without an
 * answer every bit reads as unknown_bits.
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
    uint8_t  last_config;   /* tx[1] of the last conversion */
    uint8_t  unknown_bits;  /* what the bits the manual calls unknown read as */
    bool     awaiting_cs_high;  /* powered up with CS low, no high seen yet */
    int      transfers_without_cs;
} mcp3008_model_t;

static void mcp3008_model_init(mcp3008_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->unknown_bits = 0xFFu;
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
    m->awaiting_cs_high = false;
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

    /* Until the chip drives DOUT, the MCU reads whatever is on the line */
    memset(rx, m->unknown_bits, len);

    if (!m->cs_active)
    {
        m->transfers_without_cs++;
        return true;
    }

    if (m->awaiting_cs_high || len != 3 || tx[0] != 0x01)
    {
        return true;
    }

    /* Decode config byte */
    uint8_t config = tx[1];
    bool single_ended = (config & 0x80u) != 0;
    uint8_t channel = (uint8_t)((config >> 4) & 0x07u);
    m->last_config = config;

    uint16_t raw;
    if (single_ended)
    {
        raw = (channel < 8) ? m->single[channel] : 0;
    }
    else
    {
        /* DS21295D Table 5-2: D2:D1 pick the pair, D0 = 1 swaps IN+ and
         * IN-. diff[] holds the even-channel-positive reading, and the
         * manual gives code 000h when IN+ <= IN-, so a swapped read is 0. */
        raw = (channel & 0x01u) ? 0 : m->diff[channel >> 1];
    }

    /* rx[0] stays unknown; rx[1] is five unknown bits, null 0, B9, B8 */
    rx[1] = (uint8_t)((m->unknown_bits & 0xF8u) | ((raw >> 8) & 0x03u));
    rx[2] = (uint8_t)(raw & 0xFFu);

    return true;
}

#endif /* HW_MODEL_MCP3008_H */
