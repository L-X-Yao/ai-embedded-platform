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
 * - VREF, PD and Gx are kept per channel
 * - An EEPROM per channel, at the factory values of Table 4-2 (VREF = 1,
 *   internal 2.048 V), uploaded to the input and output registers at power
 *   on. Multi-Write leaves it alone
 * - The output register follows the input register when UDAC = 0 or the LDAC
 *   pin is low
 * - A read gives the frame of Figure 5-15
 *
 * Validates command byte encoding against MCP4728 protocol.
 */

#ifndef HW_MODEL_MCP4728_H
#define HW_MODEL_MCP4728_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MCP4728_MODEL_NUM_CH  4

/* Bytes of one channel in a read: input register, then EEPROM */
#define MCP4728_MODEL_READ_PER_CH  6u

typedef struct {
    uint16_t channel_value[MCP4728_MODEL_NUM_CH];  /* DAC input register */
    uint8_t  channel_vref[MCP4728_MODEL_NUM_CH];  /* 0 = VDD, 1 = internal */
    uint8_t  channel_pd[MCP4728_MODEL_NUM_CH];    /* PD1 PD0 */
    uint8_t  channel_gain[MCP4728_MODEL_NUM_CH];  /* 0 = x1, 1 = x2 */
    uint16_t channel_output[MCP4728_MODEL_NUM_CH]; /* what VOUT shows */
    uint16_t eeprom_value[MCP4728_MODEL_NUM_CH];
    uint8_t  eeprom_vref[MCP4728_MODEL_NUM_CH];
    uint8_t  eeprom_pd[MCP4728_MODEL_NUM_CH];
    uint8_t  eeprom_gain[MCP4728_MODEL_NUM_CH];
    bool     ldac_low;                             /* LDAC pin held low */
    int      write_count;
    int      read_count;
    bool     fail_next;
} mcp4728_model_t;

/* p23: "each channel uploads all configuration and DAC input codes from
 * EEPROM", and "into both of the DAC input and output registers
 * simultaneously" */
__attribute__((unused))
static void mcp4728_model_power_on(mcp4728_model_t *m)
{
    memcpy(m->channel_value, m->eeprom_value, sizeof(m->channel_value));
    memcpy(m->channel_output, m->eeprom_value, sizeof(m->channel_output));
    memcpy(m->channel_vref, m->eeprom_vref, sizeof(m->channel_vref));
    memcpy(m->channel_pd, m->eeprom_pd, sizeof(m->channel_pd));
    memcpy(m->channel_gain, m->eeprom_gain, sizeof(m->channel_gain));
}

static void mcp4728_model_init(mcp4728_model_t *m)
{
    memset(m, 0, sizeof(*m));
    memset(m->eeprom_vref, 1, sizeof(m->eeprom_vref));  /* Table 4-2 */
    mcp4728_model_power_on(m);
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

        /* p39: "If UDAC = 0 or LDAC Pin = 0: VOUT is updated after the 4th
         * byte's ACK is issued." */
        if ((cmd & 0x01u) == 0u || m->ldac_low) {
            m->channel_output[channel] = m->channel_value[channel];
        }

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
    (void)reg;  /* a read has no register pointer: it starts at channel A */
    mcp4728_model_t *m = (mcp4728_model_t *)ctx;
    m->read_count++;

    /* p45 Figure 5-15: per channel, input register then EEPROM, three bytes
     * each. The frame repeats if the master keeps reading. */
    for (uint16_t i = 0; i < len; i++) {
        unsigned ch   = (i / MCP4728_MODEL_READ_PER_CH) % MCP4728_MODEL_NUM_CH;
        unsigned pos  = i % MCP4728_MODEL_READ_PER_CH;
        bool eeprom   = pos >= 3u;
        uint16_t code = eeprom ? m->eeprom_value[ch] : m->channel_value[ch];
        uint8_t vref  = eeprom ? m->eeprom_vref[ch] : m->channel_vref[ch];
        uint8_t pd    = eeprom ? m->eeprom_pd[ch] : m->channel_pd[ch];
        uint8_t gain  = eeprom ? m->eeprom_gain[ch] : m->channel_gain[ch];

        switch (pos % 3u) {
        case 0:  /* RDY/BSY POR DAC1 DAC0 0 A2 A1 A0 */
            buf[i] = (uint8_t)(0xC0u | (ch << 4) | (dev_addr & 0x07u));
            break;
        case 1:  /* VREF PD1 PD0 Gx D11-D8 */
            buf[i] = (uint8_t)(((unsigned)vref << 7) | ((unsigned)pd << 5) |
                               ((unsigned)gain << 4) | ((unsigned)code >> 8));
            break;
        default: /* D7-D0 */
            buf[i] = (uint8_t)(code & 0xFFu);
            break;
        }
    }
    return true;
}

#endif /* HW_MODEL_MCP4728_H */
