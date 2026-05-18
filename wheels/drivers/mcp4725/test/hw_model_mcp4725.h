/*
 * hw_model_mcp4725.h — MCP4725 hardware model from datasheet
 *
 * Models Microchip DS22039D behavior:
 * - 12-bit DAC register (volatile, written by fast/DAC commands)
 * - 12-bit EEPROM register (non-volatile, written by EEPROM command)
 * - Power-down modes for DAC and EEPROM
 * - RDY/BSY flag for EEPROM write status
 * - Command-based I2C protocol (not register-addressed)
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

static void mcp4725_model_init(mcp4725_model_t *m)
{
    memset(m, 0, sizeof(*m));
}

static void mcp4725_model_set_eeprom(mcp4725_model_t *m,
                                     uint16_t val, uint8_t pd)
{
    m->eeprom_value = val & 0x0FFFu;
    m->eeprom_pd    = pd & 0x03u;
}

__attribute__((unused))
static bool mcp4725_model_write(void *ctx, uint8_t addr,
                                const uint8_t *data, uint16_t len)
{
    (void)addr;
    mcp4725_model_t *m = (mcp4725_model_t *)ctx;
    m->write_count++;

    if (len == 2) {
        /* Fast mode: [00][PD1:PD0][D11:D8] [D7:D0] */
        m->dac_pd    = (data[0] >> 4) & 0x03u;
        m->dac_value = (uint16_t)(((uint16_t)(data[0] & 0x0Fu) << 8) |
                                  (uint16_t)data[1]);
    } else if (len == 3) {
        uint8_t cmd = data[0] & 0xE0u;
        uint8_t pd  = (data[0] >> 2) & 0x03u;
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
        /* Byte 0: [RDY/BSY][POR][C1:C0][PD1:PD0][D11:D10] */
        uint8_t rdy = m->eeprom_busy ? 0x00u : 0x80u;
        data[0] = (uint8_t)(rdy | ((unsigned)m->dac_pd << 2) |
                            ((m->dac_value >> 10) & 0x03u));
        /* Byte 1: [D9:D2] */
        data[1] = (uint8_t)((m->dac_value >> 2) & 0xFFu);
        /* Byte 2: [D1:D0][000000] */
        data[2] = (uint8_t)((m->dac_value & 0x03u) << 6);
        /* Byte 3: [x][PD1e:PD0e][x][D11e:D8e] */
        data[3] = (uint8_t)((m->eeprom_pd << 5) |
                            ((m->eeprom_value >> 8) & 0x0Fu));
        /* Byte 4: [D7e:D0e] */
        data[4] = (uint8_t)(m->eeprom_value & 0xFFu);
    }
    return true;
}

#endif /* HW_MODEL_MCP4725_H */
