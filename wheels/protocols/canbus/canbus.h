// SPDX-License-Identifier: MIT
/*
 * canbus — CAN 2.0A/B frame encoder/decoder with signal extraction
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Stateless frame packing, unpacking, ID filtering, and bit-level
 * signal extract/pack for DBC-style decoding. Handles both standard
 * (11-bit) and extended (29-bit) CAN identifiers.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef CANBUS_H
#define CANBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CANBUS_OK           =  0,
    CANBUS_ERR_PARAM   = -1,
    CANBUS_ERR_SIZE     = -2,
    CANBUS_ERR_ID       = -3,
    CANBUS_ERR_DLC      = -4,
    CANBUS_ERR_OVERFLOW = -5,
} canbus_err_t;

#define CANBUS_STD_ID_MAX   0x7FFu      /* 11-bit */
#define CANBUS_EXT_ID_MAX   0x1FFFFFFFu /* 29-bit */
#define CANBUS_DLC_MAX      8u

/*
 * Wire format (controller-agnostic, for SPI CAN controllers like MCP2515):
 *
 * Standard frame (no RTR):
 *   [FLAGS_DLC][ID_H][ID_L][data_0..data_N]    total = 3 + dlc
 *
 * Extended frame (no RTR):
 *   [FLAGS_DLC][ID3][ID2][ID1][ID0][data_0..data_N]  total = 5 + dlc
 *
 * FLAGS_DLC byte (always first — enables unambiguous type detection):
 *   bit 7: extended flag
 *   bit 6: RTR flag
 *   bits 3-0: DLC (0-8)
 *
 * ID bytes: big-endian, standard uses 2 bytes, extended uses 4 bytes.
 * RTR frames carry no data (dlc indicates requested length).
 */
#define CANBUS_PACK_STD_OVERHEAD  3u  /* 2 ID + 1 flags/dlc */
#define CANBUS_PACK_EXT_OVERHEAD  5u  /* 4 ID + 1 flags/dlc */
#define CANBUS_PACK_MAX_SIZE      13u /* 5 + 8 */

#define CANBUS_FLAG_EXTENDED  0x80u
#define CANBUS_FLAG_RTR       0x40u

typedef struct {
    uint32_t id;            /* 11-bit (standard) or 29-bit (extended) */
    uint8_t  data[8];
    uint8_t  dlc;           /* 0-8 */
    bool     extended;      /* true = 29-bit ID (CAN 2.0B) */
    bool     rtr;           /* remote transmission request */
} canbus_frame_t;

/** Validate frame fields. @thread_safe: yes (pure function) */
canbus_err_t canbus_frame_validate(const canbus_frame_t *frame);

/**
 * Serialize frame to raw bytes (controller-agnostic wire format).
 * @return number of bytes written, or negative canbus_err_t on error.
 * @thread_safe: yes (pure function)
 */
int canbus_frame_pack(const canbus_frame_t *frame,
                      uint8_t *buf, uint16_t buf_size);

/**
 * Deserialize raw bytes to frame.
 * @thread_safe: yes (pure function)
 */
canbus_err_t canbus_frame_unpack(const uint8_t *buf, uint16_t len,
                                 canbus_frame_t *frame);

/** CAN ID filter for RX message filtering. */
typedef struct {
    uint32_t id;
    uint32_t mask;          /* 1 = must match, 0 = don't care */
    bool     extended;
} canbus_filter_t;

/** Check if a frame matches the filter. @thread_safe: yes (pure function) */
bool canbus_filter_match(const canbus_filter_t *filter,
                         const canbus_frame_t *frame);

/**
 * Extract a signal from CAN data (DBC-style bit-level decoding).
 * @param data     CAN frame data bytes (8 bytes)
 * @param start_bit bit position (0-63)
 * @param length   signal length in bits (1-64)
 * @param big_endian true for Motorola byte order, false for Intel
 * @return extracted value (zero-extended)
 * @thread_safe: yes (pure function)
 */
uint64_t canbus_signal_extract(const uint8_t *data, uint8_t start_bit,
                               uint8_t length, bool big_endian);

/**
 * Pack a signal into CAN data (DBC-style bit-level encoding).
 * @param data     CAN frame data bytes (8 bytes, modified in place)
 * @param start_bit bit position (0-63)
 * @param length   signal length in bits (1-64)
 * @param big_endian true for Motorola byte order, false for Intel
 * @param value    value to pack
 * @thread_safe: yes (pure function)
 */
void canbus_signal_pack(uint8_t *data, uint8_t start_bit,
                        uint8_t length, bool big_endian, uint64_t value);

/** @return true if id fits in 11 bits (standard CAN). */
bool canbus_is_standard_id(uint32_t id);

/** @return true if id fits in 29 bits (extended CAN). */
bool canbus_is_extended_id(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* CANBUS_H */
