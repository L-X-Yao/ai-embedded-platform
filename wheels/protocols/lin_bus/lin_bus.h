// SPDX-License-Identifier: MIT
/*
 * lin_bus -- LIN bus protocol frame encoding/decoding
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef LIN_BUS_H
#define LIN_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define LIN_BUS_SYNC_BYTE     0x55
#define LIN_BUS_MAX_DATA_LEN  8
#define LIN_BUS_MAX_ID        63

/* Encoded frame: sync(1) + PID(1) + data(1-8) + checksum(1) = 4..11 bytes */
#define LIN_BUS_MAX_FRAME_LEN 11

/* ID ranges */
#define LIN_BUS_ID_SIGNAL_MAX   59   /* 0-59: signal carrying frames */
#define LIN_BUS_ID_DIAG_REQ     60   /* diagnostic request */
#define LIN_BUS_ID_DIAG_RESP    61   /* diagnostic response */
#define LIN_BUS_ID_RESERVED_62  62   /* reserved */
#define LIN_BUS_ID_RESERVED_63  63   /* reserved */

/* ---------- error codes ---------- */

typedef enum {
    LIN_BUS_OK             =  0,
    LIN_BUS_ERR_PARAM      = -1,
    LIN_BUS_ERR_SYNC       = -2,
    LIN_BUS_ERR_PARITY     = -3,
    LIN_BUS_ERR_CHECKSUM   = -4,
    LIN_BUS_ERR_FORMAT     = -5,
} lin_bus_err_t;

/* ---------- checksum type ---------- */

typedef enum {
    LIN_BUS_CHECKSUM_CLASSIC,     /* LIN 1.x: sum of data bytes only */
    LIN_BUS_CHECKSUM_ENHANCED,    /* LIN 2.0+: sum of PID + data bytes */
} lin_bus_checksum_type_t;

/* ---------- frame structure ---------- */

typedef struct {
    uint8_t                  id;            /* 0-63 */
    uint8_t                  data[LIN_BUS_MAX_DATA_LEN];
    uint8_t                  data_len;      /* 1-8 */
    lin_bus_checksum_type_t  checksum_type;
} lin_bus_frame_t;

/* ---------- API ---------- */

/** Calculate PID from ID (6 bits + 2 parity bits).
 *  @param id  Frame ID (0-63)
 *  @return PID byte with parity bits set, or 0 if id > 63
 *  @thread_safe: yes  @isr_safe: yes */
uint8_t lin_bus_calc_pid(uint8_t id);

/** Verify PID parity bits.
 *  @param pid  PID byte to verify
 *  @return true if parity is correct
 *  @thread_safe: yes  @isr_safe: yes */
bool lin_bus_verify_pid(uint8_t pid);

/** Calculate checksum for a frame.
 *  @param frame  Frame with id, data, data_len, checksum_type set
 *  @return Checksum byte, or 0 if frame is NULL
 *  @thread_safe: yes  @isr_safe: yes */
uint8_t lin_bus_calc_checksum(const lin_bus_frame_t *frame);

/** Encode a LIN frame into a buffer.
 *  Output: [sync=0x55] [PID] [data...] [checksum]
 *  Break field is at UART level, not included.
 *  @param frame   Frame to encode
 *  @param buf     Output buffer
 *  @param cap     Capacity of output buffer
 *  @param len     Receives actual bytes written
 *  @return LIN_BUS_OK on success
 *  @thread_safe: yes  @isr_safe: no */
lin_bus_err_t lin_bus_encode(const lin_bus_frame_t *frame,
                             uint8_t *buf, size_t cap, size_t *len);

/** Decode a LIN frame from a buffer.
 *  Input must be: [sync=0x55] [PID] [data...] [checksum]
 *  Verifies sync, PID parity, and checksum.
 *  @param buf     Input buffer
 *  @param len     Length of input
 *  @param frame   Receives decoded frame
 *  @return LIN_BUS_OK on success
 *  @thread_safe: yes  @isr_safe: no */
lin_bus_err_t lin_bus_decode(const uint8_t *buf, size_t len,
                             lin_bus_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* LIN_BUS_H */
