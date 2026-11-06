// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp4725.h — MCP4725 hardware model from datasheet
 *
 * Models Microchip DS22039D behavior:
 * - 12-bit DAC register (volatile, written by fast/DAC commands)
 * - 12-bit EEPROM register (non-volatile, written by EEPROM command)
 * - Power-down modes for DAC and EEPROM
 * - RDY/BSY flag for EEPROM write status: an EEPROM write goes busy until
 *   mcp4725_model_finish_eeprom(), and every write is ignored meanwhile
 *   (p.25 Note 1)
 * - Factory EEPROM at mid-scale, loaded into the DAC at reset (p.19)
 * - Command-based I2C protocol (not register-addressed): cut-short frames
 *   and reserved commands are ignored (p.23)
 *
 * Unlike register-based hw_models, MCP4725 uses raw I2C write/read,
 * so this model implements the write/read callbacks directly.
 */

#ifndef HW_MODEL_MCP4725_H
#define HW_MODEL_MCP4725_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint16_t dac_value;
    uint8_t  dac_pd;
    uint16_t eeprom_value;
    uint8_t  eeprom_pd;
    bool     eeprom_busy;
    int      write_count;
    int      read_count;
} mcp4725_model_t;

/* p.19 "In the Reset conditions, the device uploads the EEPROM data into
 * the DAC register." */
static void mcp4725_model_reset(mcp4725_model_t *m)
{
    m->dac_value   = m->eeprom_value;
    m->dac_pd      = m->eeprom_pd;
    m->eeprom_busy = false;
}

static void mcp4725_model_init(mcp4725_model_t *m)
{
    memset(m, 0, sizeof(*m));
    /* p.21 Table 5-3 note 2: "Bit D11 = '1' (while all other bits are "0")" */
    m->eeprom_value = 0x800u;
    mcp4725_model_reset(m);
}

/* A part stored with this EEPROM content, after its reset. */
static void mcp4725_model_set_eeprom(mcp4725_model_t *m,
                                     uint16_t val, uint8_t pd)
{
    m->eeprom_value = val & 0x0FFFu;
    m->eeprom_pd    = pd & 0x03u;
    mcp4725_model_reset(m);
}

/* The EEPROM cycle ends: p.4 TWRITE is 25 ms typical, 50 ms maximum. */
__attribute__((unused))
static void mcp4725_model_finish_eeprom(mcp4725_model_t *m)
{
    m->eeprom_busy = false;
}

__attribute__((unused))
static bool mcp4725_model_write(void *ctx, uint8_t addr,
                                const uint8_t *data, uint16_t len)
{
    (void)addr;
    mcp4725_model_t *m = (mcp4725_model_t *)ctx;
    m->write_count++;

    /* p.25 Note 1 "Any new write command including repeat bytes during the
     * EEPROM write mode is ignored." */
    if (m->eeprom_busy)
        return true;

    if (len == 2) {
        /* Only Fast mode fits in two bytes: p.23 a cut-short Write DAC frame
         * is ignored, and so are the "reserved" commands. */
        if (data[0] & 0xC0u)
            return true;

        /* Fast mode: [00][PD1:PD0][D11:D8] [D7:D0] */
        m->dac_pd    = (data[0] >> 4) & 0x03u;
        m->dac_value = (uint16_t)(((uint16_t)(data[0] & 0x0Fu) << 8) |
                                  (uint16_t)data[1]);
    } else if (len == 3) {
        uint8_t cmd = data[0] & 0xE0u;
        /* Figure 6-2: C2 C1 C0 X X PD1 PD0 X */
        uint8_t pd  = (data[0] >> 1) & 0x03u;
        uint16_t val = (uint16_t)(((uint16_t)data[1] << 4) |
                                  ((uint16_t)data[2] >> 4));

        if (cmd == 0x40u) {
            /* Write DAC register only */
            m->dac_value = val;
            m->dac_pd    = pd;
        } else if (cmd == 0x60u) {
            /* Write DAC register + EEPROM */
            m->dac_value    = val;
            m->dac_pd       = pd;
            m->eeprom_value = val;
            m->eeprom_pd    = pd;
            m->eeprom_busy  = true;
        }
    }
    return true;
}

__attribute__((unused))
static bool mcp4725_model_read(void *ctx, uint8_t addr,
                               uint8_t *data, uint16_t len)
{
    (void)addr;
    mcp4725_model_t *m = (mcp4725_model_t *)ctx;
    m->read_count++;

    if (len >= 5) {
        /* Figure 6-3, after the address byte.
         * Byte 0: [RDY/BSY][POR][x][x][x][PD1][PD0][x]
         * Table 5-4 names POR but gives no level. It reads 1 here, so a
         * driver that leaves it unmasked shows up. */
        uint8_t rdy = m->eeprom_busy ? 0x00u : 0x80u;
        data[0] = (uint8_t)(rdy | 0x40u | ((unsigned)m->dac_pd << 1));
        /* Byte 1: [D11:D4] */
        data[1] = (uint8_t)((m->dac_value >> 4) & 0xFFu);
        /* Byte 2: [D3:D0][xxxx] */
        data[2] = (uint8_t)((m->dac_value & 0x0Fu) << 4);
        /* Byte 3: [x][PD1e:PD0e][x][D11e:D8e] */
        data[3] = (uint8_t)((m->eeprom_pd << 5) |
                            ((m->eeprom_value >> 8) & 0x0Fu));
        /* Byte 4: [D7e:D0e] */
        data[4] = (uint8_t)(m->eeprom_value & 0xFFu);
    }
    return true;
}

#endif /* HW_MODEL_MCP4725_H */
