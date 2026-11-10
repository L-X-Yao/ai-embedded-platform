// SPDX-License-Identifier: MIT
/*
 * mavlink -- MAVLink v2 frame parser and serializer
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Minimal MAVLink v2 framing layer: byte-at-a-time parser, frame
 * serializer, CRC-16/MCRF4XX, and CRC_EXTRA lookup for common
 * messages (HEARTBEAT, SYS_STATUS, ATTITUDE, GLOBAL_POSITION_INT,
 * GPS_RAW_INT).
 *
 * Does NOT implement the full auto-generated message set -- only the
 * framing protocol and a handful of hardcoded message IDs. Any other
 * message needs its CRC_EXTRA from the caller, through
 * mavlink_parser_set_crc_table(): without it the parser cannot check
 * the frame and reports MAVLINK_PARSE_UNKNOWN_MSG.
 * Pure C99, no heap, all buffers caller-provided.
 */

#ifndef MAVLINK_H
#define MAVLINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define MAVLINK_STX             0xFD
#define MAVLINK_MAX_PAYLOAD     255
#define MAVLINK_HEADER_LEN      10   /* STX + LEN + flags(2) + SEQ + SYS + COMP + MSGID(3) */
#define MAVLINK_CHECKSUM_LEN    2
#define MAVLINK_FRAME_MAX       (MAVLINK_HEADER_LEN + MAVLINK_MAX_PAYLOAD + MAVLINK_CHECKSUM_LEN)

/* Well-known message IDs */
#define MAVLINK_MSG_ID_HEARTBEAT            0
#define MAVLINK_MSG_ID_SYS_STATUS           1
#define MAVLINK_MSG_ID_GPS_RAW_INT          24
#define MAVLINK_MSG_ID_ATTITUDE             30
#define MAVLINK_MSG_ID_GLOBAL_POSITION_INT  33

/* ---------- error codes ---------- */

typedef enum {
    MAVLINK_OK          =  0,
    MAVLINK_ERR_PARAM   = -1,
    MAVLINK_ERR_SIZE    = -2,
    MAVLINK_ERR_CRC     = -3,
} mavlink_err_t;

/* ---------- parse states ---------- */

typedef enum {
    MAVLINK_PARSE_IDLE,
    MAVLINK_PARSE_GOT_STX,
    MAVLINK_PARSE_IN_PROGRESS,
    MAVLINK_PARSE_COMPLETE,
    MAVLINK_PARSE_ERROR,
    /* A whole frame whose CRC_EXTRA the parser does not know, so its CRC cannot
     * be checked and it is not valid: mavlink_get_msg() returns NULL. Apart
     * from an ERROR it says the link is fine and a table entry is missing.
     * parser->msg.msgid names the message, for a log only. */
    MAVLINK_PARSE_UNKNOWN_MSG,
} mavlink_parse_state_t;

/* ---------- message struct ---------- */

typedef struct {
    uint8_t  stx;
    uint8_t  len;
    uint8_t  incompat_flags;
    uint8_t  compat_flags;
    uint8_t  seq;
    uint8_t  sysid;
    uint8_t  compid;
    uint32_t msgid;             /* 24-bit */
    uint8_t  payload[MAVLINK_MAX_PAYLOAD];
    uint16_t checksum;
} mavlink_msg_t;

/* ---------- CRC_EXTRA table ---------- */

/** One message the caller can validate. Take crc_extra from the dialect, as
 *  the MAVLINK_MESSAGE_CRCS list of a generated c_library_v2 gives it. */
typedef struct {
    uint32_t msgid;
    uint8_t  crc_extra;
} mavlink_crc_extra_t;

/* ---------- parser context ---------- */

typedef struct {
    mavlink_parse_state_t state;
    mavlink_msg_t         msg;
    uint16_t              rx_count;  /* bytes received after STX */
    uint16_t              crc;       /* running CRC */
    const mavlink_crc_extra_t *crc_table;  /* caller's, see set_crc_table */
    uint16_t              crc_table_len;
    uint8_t               extra_known;     /* this frame's CRC_EXTRA was found */
} mavlink_parser_t;

/* ---------- API ---------- */

/** Initialize parser to idle state. @thread_safe: no @isr_safe: no */
mavlink_err_t mavlink_parser_init(mavlink_parser_t *parser);

/** Give the parser the CRC_EXTRA of messages beyond the five it knows.
 *  An entry wins over a built-in value, and 0 is a value like any other.
 *  The table stays the caller's and must outlive the parser. It is searched
 *  in order once per frame, so a long table costs time at the frame's end.
 *  mavlink_parser_init() clears it, so call this after init. NULL with
 *  count 0 removes the table; NULL with any other count is MAVLINK_ERR_PARAM.
 *  @thread_safe: caller_must_lock @isr_safe: no */
mavlink_err_t mavlink_parser_set_crc_table(mavlink_parser_t *parser,
                                           const mavlink_crc_extra_t *table,
                                           uint16_t count);

/** Feed one byte to the parser. Returns current parse state.
 *  When MAVLINK_PARSE_COMPLETE is returned, call mavlink_get_msg()
 *  to retrieve the parsed message. MAVLINK_PARSE_UNKNOWN_MSG means a whole
 *  frame went by that nothing can validate. Parser auto-resets on the next
 *  call after COMPLETE, ERROR or UNKNOWN_MSG.
 *  @thread_safe: caller_must_lock @isr_safe: no */
mavlink_parse_state_t mavlink_parse_byte(mavlink_parser_t *parser, uint8_t byte);

/** Get pointer to parsed message (valid only when state == COMPLETE).
 *  @thread_safe: caller_must_lock @isr_safe: no */
const mavlink_msg_t *mavlink_get_msg(const mavlink_parser_t *parser);

/** Serialize a message into buf. Writes header + payload + CRC.
 *  Caller must set msg fields: len, sysid, compid, seq, msgid, payload.
 *  crc_extra is the per-message CRC seed byte.
 *  Returns MAVLINK_OK on success; *out_len set to bytes written.
 *  @thread_safe: yes (pure function) @isr_safe: no */
mavlink_err_t mavlink_serialize(const mavlink_msg_t *msg,
                                uint8_t *buf, uint16_t buf_len,
                                uint16_t *out_len, uint8_t crc_extra);

/** Compute CRC-16/MCRF4XX (X.25) over data[0..len-1].
 *  @thread_safe: yes (pure function) @isr_safe: yes */
uint16_t mavlink_crc16(const uint8_t *data, uint16_t len, uint16_t init);

/** Return the built-in CRC_EXTRA of a message, or 0 if there is none. This
 *  does not see a table given to a parser. A built-in value is never 0.
 *  @thread_safe: yes (pure function) @isr_safe: yes */
uint8_t mavlink_msg_crc_extra(uint32_t msgid);

#ifdef __cplusplus
}
#endif

#endif /* MAVLINK_H */
