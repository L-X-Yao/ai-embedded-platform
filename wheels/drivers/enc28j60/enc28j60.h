// SPDX-License-Identifier: MIT
/*
 * enc28j60 -- SPI Ethernet controller driver for Microchip ENC28J60
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal
 */

#ifndef ENC28J60_H
#define ENC28J60_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ENC28J60_OK            =  0,
    ENC28J60_ERR_SPI       = -1,
    ENC28J60_ERR_PARAM    = -2,
    ENC28J60_ERR_TIMEOUT   = -3,
    ENC28J60_ERR_SIZE      = -4,
    ENC28J60_ERR_NO_PACKET = -5,
} enc28j60_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} enc28j60_spi_t;

/* ---------- constants ---------- */

#define ENC28J60_MAX_FRAME  1518U

/* ---------- device handle ---------- */

typedef struct {
    const enc28j60_spi_t *spi;
    uint8_t  mac_addr[6];
    uint16_t next_packet_ptr;
    uint8_t  bank;
} enc28j60_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/**
 * Soft-reset, configure buffers, set MAC, enable RX.
 * @thread_safe: no
 */
enc28j60_err_t enc28j60_init(enc28j60_t *dev, const enc28j60_spi_t *spi,
                              const uint8_t *mac_addr);

/**
 * Transmit a single Ethernet frame.
 * @thread_safe: caller_must_lock
 */
enc28j60_err_t enc28j60_send(enc28j60_t *dev, const uint8_t *frame,
                              uint16_t len);

/**
 * Receive one Ethernet frame from RX buffer.
 * @thread_safe: caller_must_lock
 */
enc28j60_err_t enc28j60_receive(enc28j60_t *dev, uint8_t *frame,
                                 uint16_t buf_size, uint16_t *frame_len);

/**
 * Check if at least one packet is pending.
 * @thread_safe: caller_must_lock
 */
bool enc28j60_has_packet(enc28j60_t *dev);

/**
 * Read silicon revision (EREVID register).
 * @thread_safe: caller_must_lock
 */
uint8_t enc28j60_get_revision(enc28j60_t *dev);

/**
 * Update MAC address registers.
 * @thread_safe: caller_must_lock
 */
enc28j60_err_t enc28j60_set_mac(enc28j60_t *dev, const uint8_t *mac_addr);

/**
 * Set ECON1.RXEN to start reception.
 * @thread_safe: caller_must_lock
 */
void enc28j60_enable_reception(enc28j60_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ENC28J60_H */
