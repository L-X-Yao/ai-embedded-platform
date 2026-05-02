/*
 * crc_any — configurable CRC calculator supporting any width 1-32 bit
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Bit-by-bit implementation. No lookup tables — trades speed for
 * flexibility and minimal ROM/RAM footprint.
 */

#include "crc_any.h"

/* ── Internal helpers ─────────────────────────────────────────────── */

/** Reflect the lowest @p bits of @p val. */
static uint32_t reflect(uint32_t val, uint8_t bits)
{
    uint32_t out = 0;
    for (uint8_t i = 0; i < bits; i++) {
        if (val & ((uint32_t)1 << i))
            out |= (uint32_t)1 << (bits - 1u - i);
    }
    return out;
}

/** Mask for the low @p width bits. */
static uint32_t width_mask(uint8_t width)
{
    if (width >= 32)
        return 0xFFFFFFFFu;
    return ((uint32_t)1 << width) - 1u;
}

/**
 * Process one byte into the running CRC register (MSB-first algorithm).
 *
 * Input byte is reflected if cfg->refin is set. The CRC register is
 * maintained in non-reflected (MSB-aligned within a uint32_t) form
 * throughout the update phase; reflection and XOR happen at finalize.
 */
static uint32_t process_byte(const crc_any_config_t *cfg,
                             uint32_t crc, uint8_t byte)
{
    uint8_t b = cfg->refin ? (uint8_t)reflect(byte, 8) : byte;
    uint32_t topbit = (uint32_t)1 << (cfg->width - 1u);

    crc ^= (uint32_t)b << (cfg->width > 8 ? cfg->width - 8u : 0u);

    for (int i = 0; i < 8; i++) {
        if (crc & topbit)
            crc = (crc << 1) ^ cfg->poly;
        else
            crc <<= 1;
    }

    return crc & width_mask(cfg->width);
}

/** Apply refout + xorout to produce the final CRC value. */
static uint32_t finalize(const crc_any_config_t *cfg, uint32_t crc)
{
    if (cfg->refout)
        crc = reflect(crc, cfg->width);
    return (crc ^ cfg->xorout) & width_mask(cfg->width);
}

/* ── Public API ───────────────────────────────────────────────────── */

uint32_t crc_any_update(const crc_any_config_t *cfg, uint32_t crc,
                        const uint8_t *data, size_t len)
{
    if (!cfg)
        return crc;
    if (len == 0)
        return crc;
    if (!data)
        return crc;
    for (size_t i = 0; i < len; i++)
        crc = process_byte(cfg, crc, data[i]);
    return crc;
}

crc_any_err_t crc_any_compute(const crc_any_config_t *cfg,
                              const uint8_t *data, size_t len,
                              uint32_t *out)
{
    if (!cfg || !out)
        return CRC_ANY_ERR_PARAM;
    if (cfg->width < 1 || cfg->width > 32)
        return CRC_ANY_ERR_PARAM;
    if (len > 0 && !data)
        return CRC_ANY_ERR_PARAM;

    uint32_t crc = cfg->init & width_mask(cfg->width);
    crc = crc_any_update(cfg, crc, data, len);
    *out = finalize(cfg, crc);
    return CRC_ANY_OK;
}

bool crc_any_verify(const crc_any_config_t *cfg,
                    const uint8_t *data, size_t len,
                    uint32_t expected)
{
    uint32_t computed;
    if (crc_any_compute(cfg, data, len, &computed) != CRC_ANY_OK)
        return false;
    return computed == expected;
}
