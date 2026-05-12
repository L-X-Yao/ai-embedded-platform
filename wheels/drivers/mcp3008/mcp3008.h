// SPDX-License-Identifier: MIT
/*
 * mcp3008 -- Microchip MCP3008 10-bit 8-channel SPI ADC driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * 10-bit resolution, 8 single-ended or 4 differential channels.
 * SPI mode 0,0. Protocol: 3-byte transfer.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the mcp3008_t struct.
 */

#ifndef MCP3008_H
#define MCP3008_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MCP3008_OK          =  0,
    MCP3008_ERR_PARAM   = -1,
    MCP3008_ERR_SPI     = -2,
} mcp3008_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void *ctx;
    bool (*cs_select)(void *ctx);
    void (*cs_deselect)(void *ctx);
    bool (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);
} mcp3008_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const mcp3008_spi_t *spi;
} mcp3008_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
mcp3008_err_t mcp3008_init(mcp3008_t *dev, const mcp3008_spi_t *spi);

/** Read single-ended channel (0-7), return 10-bit raw value.
 *  @thread_safe: no @isr_safe: no */
mcp3008_err_t mcp3008_read_single(mcp3008_t *dev, uint8_t channel,
                                  uint16_t *raw);

/** Read differential pair (0-3), return 10-bit raw value.
 *  Pairs: 0=CH0-CH1, 1=CH2-CH3, 2=CH4-CH5, 3=CH6-CH7.
 *  @thread_safe: no @isr_safe: no */
mcp3008_err_t mcp3008_read_diff(mcp3008_t *dev, uint8_t diff_pair,
                                uint16_t *raw);

/** Read single-ended channel and convert to voltage.
 *  @param vref reference voltage (must be > 0).
 *  @thread_safe: no @isr_safe: no */
mcp3008_err_t mcp3008_read_voltage(mcp3008_t *dev, uint8_t channel,
                                   float vref, float *voltage);

#ifdef __cplusplus
}
#endif

#endif /* MCP3008_H */
