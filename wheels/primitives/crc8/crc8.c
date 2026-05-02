// SPDX-License-Identifier: MIT
/*
 * crc8 — CRC-8 calculator with multiple polynomial support (bit-by-bit)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Bit-by-bit computation: no lookup table, saves 256 bytes Flash on MCU.
 */

#include "crc8.h"

/* --- Pre-defined configurations ----------------------------------------- */

const crc8_config_t CRC8_DEFAULT   = { 0x07, 0x00, false, false, 0x00 };
const crc8_config_t CRC8_MAXIM     = { 0x31, 0x00, true,  true,  0x00 };
const crc8_config_t CRC8_SENSIRION = { 0x31, 0xFF, false, false, 0x00 };
const crc8_config_t CRC8_SMBUS     = { 0x07, 0x00, false, false, 0x00 };
const crc8_config_t CRC8_ROHC      = { 0x07, 0xFF, true,  true,  0x00 };

/* --- Helpers ------------------------------------------------------------ */

/**
 * Reflect (bit-reverse) an 8-bit value.
 */
static uint8_t reflect8(uint8_t val)
{
    uint8_t out = 0;
    for (int i = 0; i < 8; i++) {
        if (val & (1u << i))
            out |= (uint8_t)(1u << (7 - i));
    }
    return out;
}

/* --- Core --------------------------------------------------------------- */

uint8_t crc8_update(const crc8_config_t *cfg,
                    uint8_t crc, const uint8_t *data, size_t len)
{
    if (!cfg || (!data && len > 0))
        return crc;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = cfg->ref_in ? reflect8(data[i]) : data[i];
        crc ^= byte;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80u)
                crc = (uint8_t)((uint8_t)(crc << 1) ^ cfg->poly);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

uint8_t crc8_compute(const crc8_config_t *cfg,
                     const uint8_t *data, size_t len)
{
    if (!cfg)
        return 0;

    uint8_t crc = crc8_update(cfg, cfg->init, data, len);

    if (cfg->ref_out)
        crc = reflect8(crc);

    return crc ^ cfg->xor_out;
}

bool crc8_verify(const crc8_config_t *cfg,
                 const uint8_t *data, size_t len, uint8_t expected)
{
    if (!cfg)
        return false;
    return crc8_compute(cfg, data, len) == expected;
}
