// SPDX-License-Identifier: MIT
/*
 * crsf — TBS Crossfire / ExpressLRS (CRSF) RC protocol
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Frame: [addr] [len] [type] [payload...] [crc8]
 * A whole frame is at most 64 bytes, so len (type + payload + crc8) is 2 to 62
 * and the payload at most 60 bytes (tbs-fpv/tbs-crsf-spec, crsf.md).
 * CRC-8 DVB-S2 (poly 0xD5) over type + payload.
 * 16 channels, 11-bit packed (range 172-1811).
 * Pure C99, no heap, no OS.
 */

#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRSF_NUM_CHANNELS   16
#define CRSF_CHANNEL_BYTES  22
#define CRSF_MAX_FRAME_SIZE 64
#define CRSF_MAX_PAYLOAD    (CRSF_MAX_FRAME_SIZE - 4)  /* less addr, len, type, crc8 */
#define CRSF_ADDR_FC        0xC8

#define CRSF_TYPE_RC_CHANNELS 0x16

typedef enum {
    CRSF_OK            =  0,
    CRSF_ERR_PARAM     = -1,
    CRSF_ERR_SHORT     = -2,
    CRSF_ERR_CRC       = -3,
    CRSF_ERR_OVERFLOW  = -4,
    CRSF_ERR_FORMAT    = -5,  /* length byte outside 2 to 62: discard the frame */
} crsf_err_t;

typedef struct {
    uint8_t  addr;
    uint8_t  type;
    uint8_t  payload[CRSF_MAX_FRAME_SIZE];  /* only CRSF_MAX_PAYLOAD can be used */
    uint8_t  payload_len;
} crsf_frame_t;

typedef struct {
    uint16_t channels[CRSF_NUM_CHANNELS];
} crsf_channels_t;

/** CRC-8 DVB-S2 (poly 0xD5). @thread_safe: yes @isr_safe: yes */
uint8_t crsf_crc8(const uint8_t *data, size_t len);

/** Decode a raw CRSF frame. CRSF_ERR_SHORT means more bytes may complete it.
 *  CRSF_ERR_FORMAT means they cannot: the length byte is outside 2 to 62, which
 *  takes two bytes of data to see. @thread_safe: yes @isr_safe: yes */
crsf_err_t crsf_decode_frame(const uint8_t *data, size_t len,
                              crsf_frame_t *out);

/** Encode a CRSF frame into buffer. Returns total bytes written. A payload_len
 *  past CRSF_MAX_PAYLOAD is CRSF_ERR_OVERFLOW. @thread_safe: yes @isr_safe: yes */
crsf_err_t crsf_encode_frame(const crsf_frame_t *frame,
                              uint8_t *buf, size_t cap, size_t *out_len);

/** Unpack 22 bytes of 11-bit channel data into 16 channels. @thread_safe: yes @isr_safe: yes */
crsf_err_t crsf_unpack_channels(const uint8_t *payload, size_t len,
                                 crsf_channels_t *out);

/** Pack 16 channels into 22 bytes. @thread_safe: yes @isr_safe: yes */
crsf_err_t crsf_pack_channels(const crsf_channels_t *channels,
                               uint8_t *payload, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* CRSF_H */
