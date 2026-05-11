// SPDX-License-Identifier: MIT
/*
 * cc1101 — TI CC1101 Sub-GHz transceiver driver (SPI)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Register-based SPI. Header byte: bit7=R/W, bit6=burst, bits5:0=address.
 * Status byte returned on every SPI header byte.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the cc1101_t struct.
 */

#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CC1101_OK           =  0,
    CC1101_ERR_PARAM    = -1,
    CC1101_ERR_SPI      = -2,
    CC1101_ERR_ID       = -3,
    CC1101_ERR_TIMEOUT  = -4,
} cc1101_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_select)(void *ctx);
    void    (*cs_deselect)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} cc1101_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const cc1101_hal_t *hal;
    uint32_t frequency;     /* Hz */
    int8_t   tx_power;      /* dBm index into PATABLE */
} cc1101_t;

/* ---------- API (all @thread_safe: caller_must_lock) @isr_safe: no ---------- */

/** Reset chip, verify PARTNUM/VERSION, enter idle. */
cc1101_err_t cc1101_init(cc1101_t *dev, const cc1101_hal_t *hal);

/** Set carrier frequency in Hz. F_XOSC = 26 MHz. */
cc1101_err_t cc1101_set_frequency(cc1101_t *dev, uint32_t freq_hz);

/** Set TX power (raw PATABLE byte). */
cc1101_err_t cc1101_set_power(cc1101_t *dev, uint8_t pa_value);

/** Transmit packet. Writes to TX FIFO, strobes STX, polls for TX complete. */
cc1101_err_t cc1101_transmit(cc1101_t *dev, const uint8_t *data, uint8_t len);

/** Receive packet. Strobes SRX, polls for RX complete, reads RX FIFO. */
cc1101_err_t cc1101_receive(cc1101_t *dev, uint8_t *buf, uint8_t cap,
                            uint8_t *out_len);

/** Read last packet RSSI in half-dBm raw value. */
int16_t cc1101_get_rssi(cc1101_t *dev);

/** Enter idle mode (SIDLE strobe). */
cc1101_err_t cc1101_idle(cc1101_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* CC1101_H */
