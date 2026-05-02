// SPDX-License-Identifier: MIT
/*
 * mcp2515 — SPI CAN bus controller driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef MCP2515_H
#define MCP2515_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MCP2515_OK          =  0,
    MCP2515_ERR_SPI     = -1,
    MCP2515_ERR_PARAM  = -2,
    MCP2515_ERR_TIMEOUT = -3,
    MCP2515_ERR_NO_MSG  = -4,
    MCP2515_ERR_TX_BUSY = -5,
} mcp2515_err_t;

/* ---------- operating modes ---------- */

typedef enum {
    MCP2515_MODE_NORMAL   = 0,
    MCP2515_MODE_SLEEP    = 1,
    MCP2515_MODE_LOOPBACK = 2,
    MCP2515_MODE_LISTEN   = 3,
    MCP2515_MODE_CONFIG   = 4,
} mcp2515_mode_t;

/* ---------- CAN bus speeds (8 MHz crystal) ---------- */

typedef enum {
    MCP2515_SPEED_125KBPS  = 0,
    MCP2515_SPEED_250KBPS  = 1,
    MCP2515_SPEED_500KBPS  = 2,
    MCP2515_SPEED_1MBPS    = 3,
} mcp2515_speed_t;

/* ---------- CAN message ---------- */

typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
    bool     extended;
    bool     rtr;
} mcp2515_msg_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} mcp2515_spi_t;

/* ---------- device handle ---------- */

typedef struct {
    const mcp2515_spi_t *spi;
    mcp2515_mode_t       mode;
} mcp2515_t;

/* ---------- configuration ---------- */

#ifndef MCP2515_MODE_TIMEOUT
#define MCP2515_MODE_TIMEOUT 100000U
#endif

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Reset chip, configure speed, enter normal mode. */
mcp2515_err_t mcp2515_init(mcp2515_t *dev, const mcp2515_spi_t *spi,
                            mcp2515_speed_t speed);

/** Switch operating mode. */
mcp2515_err_t mcp2515_set_mode(mcp2515_t *dev, mcp2515_mode_t mode);

/** Transmit a CAN frame (standard or extended). */
mcp2515_err_t mcp2515_send(mcp2515_t *dev, const mcp2515_msg_t *msg);

/** Receive a CAN frame from RX buffer 0 or 1. */
mcp2515_err_t mcp2515_receive(mcp2515_t *dev, mcp2515_msg_t *msg);

/** Check if any message is pending in RX buffers. */
bool mcp2515_has_message(mcp2515_t *dev);

/** Configure one of 6 acceptance filters (0-5). Must be in config mode. */
mcp2515_err_t mcp2515_set_filter(mcp2515_t *dev, uint8_t filter_num,
                                  uint32_t id, bool extended);

/** Configure one of 2 acceptance masks (0-1). Must be in config mode. */
mcp2515_err_t mcp2515_set_mask(mcp2515_t *dev, uint8_t mask_num,
                                uint32_t mask);

/** Read status register (quick poll: TX/RX flags). */
uint8_t mcp2515_read_status(mcp2515_t *dev);

/** Read error counter byte (TEC high nibble, REC low nibble from EFLG). */
uint8_t mcp2515_read_errors(mcp2515_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MCP2515_H */
