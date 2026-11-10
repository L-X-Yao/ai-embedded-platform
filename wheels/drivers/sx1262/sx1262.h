// SPDX-License-Identifier: MIT
/*
 * sx1262 — SPI LoRa radio transceiver driver for Semtech SX1262
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Command-based SPI interface (not register-based). Each SPI transaction:
 * cs_select, transfer command+data, cs_deselect. Busy-pin polling between
 * commands. Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the sx1262_t struct.
 */

#ifndef SX1262_H
#define SX1262_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SX1262_OK           =  0,
    SX1262_ERR_PARAM    = -1,
    SX1262_ERR_SPI      = -2,
    SX1262_ERR_BUSY     = -3,
    SX1262_ERR_TIMEOUT  = -4,
    SX1262_ERR_CRC      = -5,
    SX1262_ERR_SIZE     = -6,  /* packet longer than the buffer */
} sx1262_err_t;

/* ---------- LoRa parameters ---------- */

typedef enum {
    SX1262_SF5  = 5,
    SX1262_SF6  = 6,
    SX1262_SF7  = 7,
    SX1262_SF8  = 8,
    SX1262_SF9  = 9,
    SX1262_SF10 = 10,
    SX1262_SF11 = 11,
    SX1262_SF12 = 12,
} sx1262_sf_t;

typedef enum {
    SX1262_BW_7K8   = 0x00,
    SX1262_BW_10K4  = 0x08,
    SX1262_BW_15K6  = 0x01,
    SX1262_BW_20K8  = 0x09,
    SX1262_BW_31K25 = 0x02,
    SX1262_BW_41K7  = 0x0A,
    SX1262_BW_62K5  = 0x03,
    SX1262_BW_125K  = 0x04,
    SX1262_BW_250K  = 0x05,
    SX1262_BW_500K  = 0x06,
} sx1262_bw_t;

typedef enum {
    SX1262_CR_4_5 = 0x01,
    SX1262_CR_4_6 = 0x02,
    SX1262_CR_4_7 = 0x03,
    SX1262_CR_4_8 = 0x04,
} sx1262_cr_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void (*cs_select)(void *ctx);
    void (*cs_deselect)(void *ctx);
    void (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    bool (*is_busy)(void *ctx);
    void (*delay_ms)(void *ctx, uint32_t ms);
    /** Optional: return true if the last SPI transfer had an error.
     *  Set to NULL if the platform cannot detect SPI errors. */
    bool (*get_spi_error)(void *ctx);
    void *ctx;
} sx1262_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const sx1262_hal_t *hal;
    bool asleep;               /* internal: BUSY is high until the chip is woken */
    uint32_t frequency;
    sx1262_sf_t sf;
    sx1262_bw_t bw;
    sx1262_cr_t cr;
    int8_t tx_power;
} sx1262_t;

/* ---------- API (all @thread_safe: caller_must_lock) @isr_safe: no ---------- */

/** Initialize chip: enter standby, verify communication, set LoRa and 14 dBm.
 *  Pulsing NRESET first is up to the caller. */
sx1262_err_t sx1262_init(sx1262_t *dev, const sx1262_hal_t *hal);

/** Set carrier frequency in Hz (150 MHz - 960 MHz). */
sx1262_err_t sx1262_set_frequency(sx1262_t *dev, uint32_t freq_hz);

/** Configure LoRa modulation parameters. */
sx1262_err_t sx1262_configure_lora(sx1262_t *dev, sx1262_sf_t sf,
                                   sx1262_bw_t bw, sx1262_cr_t cr);

/** Transmit packet (blocks until TX done or timeout). Max 255 bytes. */
sx1262_err_t sx1262_transmit(sx1262_t *dev, const uint8_t *data, uint8_t len);

/** Receive packet (blocks until RX done or timeout).
 *  Returns SX1262_ERR_CRC for a packet that arrived damaged.
 *  A packet longer than cap is dropped, never cut: returns SX1262_ERR_SIZE,
 *  copies nothing and sets *out_len to the real length, which is then more
 *  than cap. A cap of 255 fits any packet. */
sx1262_err_t sx1262_receive(sx1262_t *dev, uint8_t *buf, uint8_t cap,
                            uint8_t *out_len);

/** Read last packet RSSI in dBm. */
int16_t sx1262_get_rssi(sx1262_t *dev);

/** Enter sleep mode. Every command except sx1262_standby then fails on BUSY. */
sx1262_err_t sx1262_sleep(sx1262_t *dev);

/** Enter standby (RC oscillator). This also wakes the chip from sleep. */
sx1262_err_t sx1262_standby(sx1262_t *dev);

/** Get chip status byte. */
uint8_t sx1262_get_status(sx1262_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SX1262_H */
