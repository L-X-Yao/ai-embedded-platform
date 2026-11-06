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
    MCP2515_SPEED_1MBPS    = 3,   /* refused: needs a faster oscillator */
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
    /* Required. Waits at least us microseconds, for the chip's Oscillator
     * Start-up Timer. Init returns MCP2515_ERR_PARAM if it is NULL. */
    void    (*delay_us)(void *ctx, uint32_t us);
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

/** Reset chip, wait out the Oscillator Start-up Timer, configure speed, enter
 *  normal mode. The wait is 128 us, which covers the slowest clock the chip
 *  allows. The power-on start-up comes before the RESET and the driver cannot
 *  know when power came up, so do not call this until the oscillator runs.
 *  Returns MCP2515_ERR_PARAM and sends nothing if spi->delay_us is NULL. */
mcp2515_err_t mcp2515_init(mcp2515_t *dev, const mcp2515_spi_t *spi,
                            mcp2515_speed_t speed);

/** Switch operating mode. Leaving Sleep wakes the chip first: this sets WAKIE
 *  and WAKIF, which pulls the INT pin low until the mode change is done, and
 *  waits 128 us for the oscillator. Frames on the bus meanwhile are ignored.
 *  The chip changes mode only once no frame is pending, so a frame that cannot
 *  go out holds it up.
 *  Returns MCP2515_ERR_TIMEOUT if the chip does not reach the mode. The request
 *  stays with the chip, which can still change mode later, so ask again. */
mcp2515_err_t mcp2515_set_mode(mcp2515_t *dev, mcp2515_mode_t mode);

/** Transmit a CAN frame (standard or extended). Returns MCP2515_ERR_PARAM in
 *  Listen-Only, Sleep and Configuration mode, where the chip sends nothing. */
mcp2515_err_t mcp2515_send(mcp2515_t *dev, const mcp2515_msg_t *msg);

/** Receive a CAN frame from RX buffer 0 or 1. */
mcp2515_err_t mcp2515_receive(mcp2515_t *dev, mcp2515_msg_t *msg);

/** Check if any message is pending in RX buffers. */
bool mcp2515_has_message(mcp2515_t *dev);

/** Configure one of 6 acceptance filters (0-5). Must be in config mode. */
mcp2515_err_t mcp2515_set_filter(mcp2515_t *dev, uint8_t filter_num,
                                  uint32_t id, bool extended);

/** Configure one of 2 acceptance masks (0-1). Must be in config mode.
 *  Set extended=true to write all 29 mask bits (SID+EID), false for 11-bit only. */
mcp2515_err_t mcp2515_set_mask(mcp2515_t *dev, uint8_t mask_num,
                                uint32_t mask, bool extended);

/** Read status register (quick poll: TX/RX flags). */
uint8_t mcp2515_read_status(mcp2515_t *dev);

/** Read the EFLG error flag byte. */
uint8_t mcp2515_read_errors(mcp2515_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MCP2515_H */
