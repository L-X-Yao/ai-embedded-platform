// SPDX-License-Identifier: MIT
/*
 * lora_sx1276 — SPI LoRa radio transceiver driver for SX1276/SX1278
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef LORA_SX1276_H
#define LORA_SX1276_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LORA_OK          =  0,
    LORA_ERR_SPI     = -1,
    LORA_ERR_PARAM  = -2,
    LORA_ERR_TIMEOUT = -3,
    LORA_ERR_CRC     = -4,
    LORA_ERR_VERSION = -5,
    LORA_ERR_SIZE    = -6,  /* packet longer than the buffer */
} lora_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*reset)(void *ctx);              /* NRESET pin toggle */
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} lora_spi_t;

/* ---------- device handle ---------- */

typedef struct {
    const lora_spi_t *spi;
    uint32_t frequency;        /* Hz (e.g., 433000000, 868000000, 915000000) */
    uint8_t  bandwidth;        /* 0-9 (7.8kHz to 500kHz)                    */
    uint8_t  spreading_factor; /* 7-12                                       */
    uint8_t  coding_rate;      /* 1-4 (4/5 to 4/8)                          */
    int8_t   tx_power;         /* dBm (2-17 for PA_BOOST, max 20)           */
} lora_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Reset chip, verify version, configure LoRa mode + frequency. */
lora_err_t lora_init(lora_t *dev, const lora_spi_t *spi, uint32_t freq_hz);

/** Set carrier frequency (Frf = freq * 2^19 / 32 MHz). */
lora_err_t lora_set_frequency(lora_t *dev, uint32_t freq_hz);

/** Set signal bandwidth (0-9). */
lora_err_t lora_set_bandwidth(lora_t *dev, uint8_t bw);

/** Set spreading factor (7-12). SF6 needs implicit header mode, which this
 *  driver does not have. */
lora_err_t lora_set_spreading_factor(lora_t *dev, uint8_t sf);

/** Set coding rate (1-4 → 4/5 to 4/8). */
lora_err_t lora_set_coding_rate(lora_t *dev, uint8_t cr);

/** Set TX output power in dBm (2-17, or up to 20 with high-power). */
lora_err_t lora_set_tx_power(lora_t *dev, int8_t dbm);

/** Transmit packet. Blocks until TxDone or timeout. */
lora_err_t lora_send(lora_t *dev, const uint8_t *data, uint8_t len);

/** Receive packet (RX single). Blocks until RxDone or timeout.
 *  A packet longer than max_len is dropped, never cut: returns LORA_ERR_SIZE,
 *  copies nothing and sets *len to the real length, which is then more than
 *  max_len. A max_len of 255 fits any packet. */
lora_err_t lora_receive(lora_t *dev, uint8_t *data, uint8_t *len,
                        uint8_t max_len);

/** Check if a received packet is available (RxDone IRQ flag set). */
bool lora_packet_available(lora_t *dev);

/** Read last packet RSSI in dBm. */
int16_t lora_get_rssi(lora_t *dev);

/** Read last packet SNR in dB. */
int8_t lora_get_snr(lora_t *dev);

/** Enter sleep mode (lowest power). */
lora_err_t lora_sleep(lora_t *dev);

/** Enter standby mode. */
lora_err_t lora_standby(lora_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX1276_H */
