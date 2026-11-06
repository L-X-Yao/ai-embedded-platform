// SPDX-License-Identifier: MIT
/*
 * rc522 — MFRC522 SPI RFID/NFC reader driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI protocol: address byte (bit7=R/W, bits6:1=register addr, bit0=0)
 * followed by data byte(s).
 * Supports ISO/IEC 14443-A card detection and UID reading.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef RC522_H
#define RC522_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    RC522_OK          =  0,
    RC522_ERR_PARAM   = -1,
    RC522_ERR_SPI     = -2,
    RC522_ERR_ID      = -3,
    RC522_ERR_TIMEOUT = -4,
    RC522_ERR_COLL    = -5,   /* collision during anticollision */
    RC522_ERR_NACK    = -6,   /* card answered HLTA: not acknowledged */
} rc522_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);   /* required for soft reset */
    void    *ctx;
} rc522_spi_t;

/* ---------- register addresses ---------- */

#define RC522_REG_COMMAND        0x01u
#define RC522_REG_COMIEN         0x02u
#define RC522_REG_DIVIEN         0x03u
#define RC522_REG_COMIRQ         0x04u
#define RC522_REG_DIVIRQ         0x05u
#define RC522_REG_ERROR          0x06u
#define RC522_REG_STATUS1        0x07u
#define RC522_REG_STATUS2        0x08u
#define RC522_REG_FIFODATA       0x09u
#define RC522_REG_FIFOLEVEL      0x0Au
#define RC522_REG_CONTROL        0x0Cu
#define RC522_REG_BITFRAMING     0x0Du
#define RC522_REG_MODE           0x11u
#define RC522_REG_TXMODE         0x12u
#define RC522_REG_TXCONTROL      0x14u
#define RC522_REG_TXASK          0x15u
#define RC522_REG_CRCRESULT_MSB  0x21u
#define RC522_REG_CRCRESULT_LSB  0x22u
#define RC522_REG_MODWIDTH       0x24u
#define RC522_REG_TMODE          0x2Au
#define RC522_REG_TPRESCALER     0x2Bu
#define RC522_REG_TRELOAD_HI     0x2Cu
#define RC522_REG_TRELOAD_LO     0x2Du
#define RC522_REG_AUTOTEST       0x36u
#define RC522_REG_VERSION        0x37u

/* ---------- commands ---------- */

#define RC522_CMD_IDLE          0x00u
#define RC522_CMD_MEM           0x01u
#define RC522_CMD_GENRANDOMID   0x02u
#define RC522_CMD_CALCCRC       0x03u
#define RC522_CMD_TRANSMIT      0x04u
#define RC522_CMD_NOCMDCHANGE   0x07u
#define RC522_CMD_RECEIVE       0x08u
#define RC522_CMD_TRANSCEIVE    0x0Cu
#define RC522_CMD_MFAUTHENT     0x0Eu
#define RC522_CMD_SOFTRESET     0x0Fu

/* ---------- PICC commands ---------- */

#define RC522_PICC_REQA         0x26u
#define RC522_PICC_WUPA         0x52u
#define RC522_PICC_ANTICOLL_CL1 0x93u
#define RC522_PICC_HALT_CMD     0x50u

/* ---------- known version IDs ---------- */

#define RC522_VERSION_1_0       0x91u
#define RC522_VERSION_2_0       0x92u

/* ---------- UID struct ---------- */

#define RC522_UID_MAX_LEN  10u

typedef struct {
    uint8_t bytes[RC522_UID_MAX_LEN];
    uint8_t len;    /* 4, 7, or 10 bytes */
} rc522_uid_t;

/* ---------- device context ---------- */

typedef struct {
    const rc522_spi_t *spi;
    uint8_t version;   /* chip version from VERSION register */
} rc522_t;

/* ---------- API ---------- */

/** Initialize, soft-reset, and verify chip version.
 *  @thread_safe: no @isr_safe: no */
rc522_err_t rc522_init(rc522_t *dev,
                           const rc522_spi_t *spi);

/** Run MFRC522 self-test (write 25 zeros to FIFO, check non-zero response).
 *  @thread_safe: no @isr_safe: no */
rc522_err_t rc522_self_test(rc522_t *dev);

/** Detect PICC using REQA. Returns OK if a card responds (ATQA received).
 *  @thread_safe: no @isr_safe: no */
rc522_err_t rc522_detect_card(rc522_t *dev);

/** Read the UID of a detected card (cascade level 1 only, 4-byte UID).
 *  Call rc522_detect_card() first.
 *  @return RC522_ERR_ID for a card with a 7 or 10 byte UID, which this driver
 *  cannot read. uid is not written.
 *  @thread_safe: no @isr_safe: no */
rc522_err_t rc522_read_card_uid(rc522_t *dev, rc522_uid_t *uid);

/** Send HLTA with CRC_A. A card halts only if it was selected first, and this
 *  driver has no SELECT, so RC522_OK does not mean the card is in HALT state:
 *  it means no card answered, which is also the result with no card present.
 *  @return RC522_ERR_NACK if a card answered, which means not acknowledged.
 *  @thread_safe: no @isr_safe: no */
rc522_err_t rc522_halt(rc522_t *dev);

/** Enable antenna (set bits in TxControlReg).
 *  @thread_safe: no @isr_safe: no */
rc522_err_t rc522_antenna_on(rc522_t *dev);

/** Get the chip version byte.
 *  @thread_safe: yes @isr_safe: yes */
uint8_t rc522_get_version(const rc522_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* RC522_H */
