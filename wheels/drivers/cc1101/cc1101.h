// SPDX-License-Identifier: MIT
/*
 * cc1101 — TI CC1101 Sub-GHz transceiver driver (SPI)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Register-based SPI protocol with header byte encoding:
 *   bit 7 = R/W (1=read, 0=write), bit 6 = burst, bits 5:0 = address.
 * Command strobes at 0x30-0x3D. TX/RX FIFO at 0x3F.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the cc1101_t struct.
 */

#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CC1101_OK           =  0,
    CC1101_ERR_PARAM    = -1,
    CC1101_ERR_SPI      = -2,
    CC1101_ERR_VERSION  = -3,
    CC1101_ERR_TIMEOUT  = -4,
} cc1101_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} cc1101_spi_t;

/* ---------- configuration ---------- */

typedef struct {
    uint32_t frequency_hz;
    uint8_t  data_rate;        /* MDMCFG4[3:0] exponent */
    uint8_t  modulation;       /* 0=2-FSK, 1=GFSK, 3=OOK */
} cc1101_cfg_t;

/* ---------- device handle ---------- */

typedef struct {
    const cc1101_spi_t *spi;
    cc1101_cfg_t        cfg;
} cc1101_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Reset chip, verify version, configure basic parameters. */
cc1101_err_t cc1101_init(cc1101_t *dev, const cc1101_spi_t *spi,
                         const cc1101_cfg_t *cfg);

/** Set carrier frequency in Hz (300-928 MHz range). */
cc1101_err_t cc1101_set_frequency(cc1101_t *dev, uint32_t freq_hz);

/** Set TX output power (raw PA table byte). */
cc1101_err_t cc1101_set_power(cc1101_t *dev, uint8_t pa_value);

/** Transmit packet (up to 61 bytes). Blocks until done or timeout. */
cc1101_err_t cc1101_transmit(cc1101_t *dev, const uint8_t *data,
                             uint8_t len);

/** Receive packet. Blocks until data available or timeout. */
cc1101_err_t cc1101_receive(cc1101_t *dev, uint8_t *buf, uint8_t capacity,
                            uint8_t *out_len);

/** Read last packet RSSI in dBm. */
cc1101_err_t cc1101_get_rssi(cc1101_t *dev, int16_t *rssi);

/** Enter IDLE state. */
cc1101_err_t cc1101_idle(cc1101_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* CC1101_H */
