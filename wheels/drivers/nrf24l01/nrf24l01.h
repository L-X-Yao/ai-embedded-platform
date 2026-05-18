// SPDX-License-Identifier: MIT
/*
 * nrf24l01 -- SPI driver for nRF24L01+ 2.4GHz radio transceiver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef NRF24L01_H
#define NRF24L01_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    NRF24_OK          =  0,
    NRF24_ERR_SPI     = -1,
    NRF24_ERR_PARAM  = -2,
    NRF24_ERR_TIMEOUT = -3,
    NRF24_ERR_MAX_RT  = -4,     /* max retransmits reached */
    NRF24_ERR_FULL    = -5,     /* TX FIFO full */
} nrf24_err_t;

/* ---------- enumerations ---------- */

typedef enum {
    NRF24_MODE_TX = 0,
    NRF24_MODE_RX = 1,
} nrf24_mode_t;

typedef enum {
    NRF24_RATE_250KBPS = 0,
    NRF24_RATE_1MBPS   = 1,
    NRF24_RATE_2MBPS   = 2,
} nrf24_data_rate_t;

typedef enum {
    NRF24_POWER_N18DBM = 0,
    NRF24_POWER_N12DBM = 1,
    NRF24_POWER_N6DBM  = 2,
    NRF24_POWER_0DBM   = 3,
} nrf24_power_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*ce_low)(void *ctx);
    void    (*ce_high)(void *ctx);
    void    (*delay_us)(void *ctx, uint32_t us);
    void    *ctx;
} nrf24_spi_t;

/* ---------- device handle ---------- */

typedef struct {
    const nrf24_spi_t *spi;
    uint8_t            channel;        /* 0-125 (2400 + channel MHz) */
    uint8_t            payload_size;   /* 1-32 bytes */
    nrf24_mode_t       mode;
} nrf24_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Initialize device: power-on, set channel/payload, clear FIFOs.
 *  @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_init(nrf24_t *dev, const nrf24_spi_t *spi,
                       uint8_t channel, uint8_t payload_size);

/** Set TX address (also sets RX_ADDR_P0 for auto-ack).
 *  @param addr_len 3-5 bytes.  @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_set_tx_addr(nrf24_t *dev, const uint8_t *addr,
                              uint8_t addr_len);

/** Set RX address for pipe 0-5.
 *  Pipes 0-1: addr_len 3-5 bytes.  Pipes 2-5: addr_len must be 1.
 *  @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_set_rx_addr(nrf24_t *dev, uint8_t pipe,
                              const uint8_t *addr, uint8_t addr_len);

/** Switch between TX and RX mode. @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_set_mode(nrf24_t *dev, nrf24_mode_t mode);

/** Set air data rate. @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_set_data_rate(nrf24_t *dev, nrf24_data_rate_t rate);

/** Set TX output power. @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_set_power(nrf24_t *dev, nrf24_power_t power);

/** Send payload (blocking: waits for TX_DS or MAX_RT).
 *  @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_send(nrf24_t *dev, const uint8_t *data, uint8_t len);

/** Read received payload. Sets *len to actual bytes read.
 *  @param max_len size of data buffer (must be >= payload_size).
 *  @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_receive(nrf24_t *dev, uint8_t *data, uint8_t max_len,
                          uint8_t *len);

/** Check if data is available in RX FIFO. */
bool nrf24_data_ready(nrf24_t *dev);

/** Flush TX FIFO. @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_flush_tx(nrf24_t *dev);

/** Flush RX FIFO. @thread_safe: caller_must_lock @isr_safe: no */
nrf24_err_t nrf24_flush_rx(nrf24_t *dev);

/** Read STATUS register. Returns 0 on NULL dev. */
uint8_t nrf24_read_status(nrf24_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* NRF24L01_H */
