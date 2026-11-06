// SPDX-License-Identifier: MIT
/*
 * w5500 -- SPI Ethernet controller driver for Wiznet W5500
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef W5500_H
#define W5500_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    W5500_OK            =  0,
    W5500_ERR_SPI       = -1,
    W5500_ERR_PARAM     = -2,
    W5500_ERR_TIMEOUT   = -3,
    W5500_ERR_SIZE      = -4,
    W5500_ERR_NO_DATA   = -5,
    W5500_ERR_SOCK      = -6,   /* invalid socket or socket not open */
} w5500_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} w5500_spi_t;

/* ---------- constants ---------- */

#define W5500_MAX_SOCK      8U
#define W5500_VERSION       0x04U

/* Socket protocols for w5500_socket_open */
#define W5500_PROTO_TCP     0x01U
#define W5500_PROTO_UDP     0x02U
#define W5500_PROTO_MACRAW  0x04U

/* Socket status values */
#define W5500_SOCK_CLOSED       0x00U
#define W5500_SOCK_INIT         0x13U
#define W5500_SOCK_LISTEN       0x14U
#define W5500_SOCK_ESTABLISHED  0x17U
#define W5500_SOCK_CLOSE_WAIT   0x1CU
#define W5500_SOCK_UDP          0x22U
#define W5500_SOCK_MACRAW       0x42U

/* TX/RX buffer size per socket (default: 2KB each) */
#ifndef W5500_SOCK_BUF_SIZE
#define W5500_SOCK_BUF_SIZE  2048U
#endif

/* ---------- device handle ---------- */

typedef struct {
    const w5500_spi_t *spi;
    uint8_t  mac_addr[6];
} w5500_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/**
 * Soft-reset, verify chip version, apply default buffer config.
 * @thread_safe: no @isr_safe: no
 */
w5500_err_t w5500_init(w5500_t *dev, const w5500_spi_t *spi);

/**
 * Write MAC address (SHAR) to common registers.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_set_mac(w5500_t *dev, const uint8_t mac[6]);

/**
 * Write IP, subnet mask, and gateway to common registers.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_set_ip(w5500_t *dev, const uint8_t ip[4],
                          const uint8_t subnet[4],
                          const uint8_t gateway[4]);

/**
 * Open a socket with given protocol and local port.
 * Protocol: W5500_PROTO_TCP, W5500_PROTO_UDP, or W5500_PROTO_MACRAW.
 * MACRAW works on socket 0 only: other sockets return W5500_ERR_SOCK.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_socket_open(w5500_t *dev, uint8_t sock,
                               uint8_t protocol, uint16_t port);

/**
 * Close a socket.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_socket_close(w5500_t *dev, uint8_t sock);

/**
 * Send data on an open socket.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_socket_send(w5500_t *dev, uint8_t sock,
                               const uint8_t *data, uint16_t len);

/**
 * Receive data from a socket.
 * On success, *out_len is set to the number of bytes actually received.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_socket_recv(w5500_t *dev, uint8_t sock,
                               uint8_t *buf, uint16_t buf_len,
                               uint16_t *out_len);

/**
 * Query socket status register.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
w5500_err_t w5500_socket_status(w5500_t *dev, uint8_t sock,
                                 uint8_t *status);

/**
 * Read chip version register (VERSIONR). Expected: 0x04.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
uint8_t w5500_read_version(w5500_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* W5500_H */
