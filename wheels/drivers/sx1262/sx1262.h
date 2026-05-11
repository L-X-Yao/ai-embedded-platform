// SPDX-License-Identifier: MIT
/*
 * sx1262 — SPI LoRa/FSK transceiver driver for Semtech SX1262
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Command-based SPI protocol (not register-based like SX1276).
 * Supports LoRa and FSK packet types.
 * Caller provides SPI, chip-select, busy-pin, and delay callbacks.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the sx1262_t struct.
 */

#ifndef SX1262_H
#define SX1262_H

#include <stdint.h>
#include <stdbool.h>

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
} sx1262_err_t;

/* ---------- packet types ---------- */

typedef enum {
    SX1262_PACKET_FSK   = 0x00,
    SX1262_PACKET_LORA  = 0x01,
} sx1262_packet_type_t;

/* ---------- LoRa spreading factor ---------- */

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

/* ---------- LoRa bandwidth ---------- */

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

/* ---------- LoRa coding rate ---------- */

typedef enum {
    SX1262_CR_4_5 = 0x01,
    SX1262_CR_4_6 = 0x02,
    SX1262_CR_4_7 = 0x03,
    SX1262_CR_4_8 = 0x04,
} sx1262_cr_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_select)(void *ctx);
    void    (*cs_deselect)(void *ctx);
    uint8_t (*transfer)(void *ctx, uint8_t tx);
    bool    (*is_busy)(void *ctx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} sx1262_spi_t;

/* ---------- configuration ---------- */

typedef struct {
    uint32_t             frequency_hz;
    int8_t               tx_power_dbm;     /* -9..+22 */
    sx1262_sf_t          spreading_factor;
    sx1262_bw_t          bandwidth;
    sx1262_cr_t          coding_rate;
} sx1262_cfg_t;

/* ---------- device handle ---------- */

typedef struct {
    const sx1262_spi_t  *spi;
    sx1262_cfg_t         cfg;
    sx1262_packet_type_t packet_type;
} sx1262_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Reset to standby, configure LoRa mode with given parameters. */
sx1262_err_t sx1262_init(sx1262_t *dev, const sx1262_spi_t *spi,
                         const sx1262_cfg_t *cfg);

/** Set RF carrier frequency in Hz. */
sx1262_err_t sx1262_set_frequency(sx1262_t *dev, uint32_t freq_hz);

/** Configure LoRa modulation parameters (SF, BW, CR). */
sx1262_err_t sx1262_configure_lora(sx1262_t *dev, sx1262_sf_t sf,
                                   sx1262_bw_t bw, sx1262_cr_t cr);

/** Transmit packet. Blocks until done or timeout. */
sx1262_err_t sx1262_transmit(sx1262_t *dev, const uint8_t *data,
                             uint8_t len);

/** Receive packet. Blocks until done or timeout. */
sx1262_err_t sx1262_receive(sx1262_t *dev, uint8_t *buf, uint8_t capacity,
                            uint8_t *out_len);

/** Read last packet RSSI in dBm (negative value). */
sx1262_err_t sx1262_get_rssi(sx1262_t *dev, int16_t *rssi);

/** Enter sleep mode (lowest power). */
sx1262_err_t sx1262_sleep(sx1262_t *dev);

/** Enter standby (RC oscillator). */
sx1262_err_t sx1262_standby(sx1262_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SX1262_H */
