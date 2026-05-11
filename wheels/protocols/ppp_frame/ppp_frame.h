// SPDX-License-Identifier: MIT
/*
 * ppp_frame -- PPP HDLC-like framing codec (RFC 1662)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef PPP_FRAME_H
#define PPP_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define PPP_FLAG      0x7E
#define PPP_ESCAPE    0x7D
#define PPP_ADDR      0xFF
#define PPP_CONTROL   0x03
#define PPP_XOR_MASK  0x20

#define PPP_FCS_INIT  0xFFFF
#define PPP_FCS_GOOD  0xF0B8  /* residue after FCS verification */

/* ---------- error codes ---------- */

typedef enum {
    PPP_FRAME_OK           =  0,
    PPP_FRAME_ERR_PARAM    = -1,
    PPP_FRAME_ERR_FCS      = -2,
    PPP_FRAME_ERR_SHORT    = -3,
    PPP_FRAME_ERR_OVERFLOW = -4,
    PPP_FRAME_ERR_FORMAT   = -5,
} ppp_frame_err_t;

/* ---------- API ---------- */

/** Compute FCS-16 (CRC-16/HDLC) over data.
 *  @thread_safe: yes  @isr_safe: yes */
uint16_t ppp_frame_fcs16(const uint8_t *data, size_t len);

/** Encode a PPP frame with HDLC byte-stuffing and FCS.
 *  Writes: flag + addr + control + protocol(2B) + payload(stuffed) + FCS(stuffed) + flag
 *  @param protocol  PPP protocol field (e.g., 0xC021 for LCP, 0x0021 for IP)
 *  @param payload   Payload bytes (may be NULL if payload_len==0)
 *  @param dst       Output buffer
 *  @param dst_cap   Capacity of output buffer
 *  @param out_len   Receives actual bytes written
 *  @return PPP_FRAME_OK on success
 *  @thread_safe: yes  @isr_safe: no */
ppp_frame_err_t ppp_frame_encode(uint16_t protocol,
                                  const uint8_t *payload, size_t payload_len,
                                  uint8_t *dst, size_t dst_cap,
                                  size_t *out_len);

/** Decode a PPP frame: remove stuffing, verify FCS, extract payload.
 *  @param src          Raw frame bytes (must start and end with 0x7E)
 *  @param src_len      Length of raw frame
 *  @param protocol     Receives the protocol field
 *  @param payload_buf  Receives the decoded payload
 *  @param payload_cap  Capacity of payload_buf
 *  @param payload_len  Receives actual payload length
 *  @return PPP_FRAME_OK on success
 *  @thread_safe: yes  @isr_safe: no */
ppp_frame_err_t ppp_frame_decode(const uint8_t *src, size_t src_len,
                                  uint16_t *protocol,
                                  uint8_t *payload_buf, size_t payload_cap,
                                  size_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif /* PPP_FRAME_H */
