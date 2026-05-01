/*
 * modbus_tcp --- Modbus TCP/IP frame encoder/decoder (MBAP + PDU)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MODBUS_TCP_OK           =  0,
    MODBUS_TCP_ERR_PARAMS   = -1,
    MODBUS_TCP_ERR_SHORT    = -2,
    MODBUS_TCP_ERR_PROTOCOL = -3,
    MODBUS_TCP_ERR_OVERFLOW = -4,
    MODBUS_TCP_ERR_EXCEPTION = -5,
} modbus_tcp_err_t;

/* ---------- constants ---------- */

#define MODBUS_TCP_MBAP_SIZE          7   /* transaction(2) + protocol(2) + length(2) + unit(1) */
#define MODBUS_TCP_PROTOCOL_ID        0   /* always 0 for Modbus */
#define MODBUS_TCP_MAX_PDU          253   /* Modbus spec max PDU */
#define MODBUS_TCP_MAX_FRAME        260   /* MBAP(7) + PDU(253) */

#define MODBUS_TCP_FC_READ_HOLDING    0x03
#define MODBUS_TCP_FC_WRITE_SINGLE    0x06
#define MODBUS_TCP_FC_WRITE_MULTIPLE  0x10

#ifndef MODBUS_TCP_MAX_READ_REGS
#define MODBUS_TCP_MAX_READ_REGS    125
#endif
#ifndef MODBUS_TCP_MAX_WRITE_REGS
#define MODBUS_TCP_MAX_WRITE_REGS   123
#endif

/* ---------- types ---------- */

/** MBAP header (7 bytes on wire). */
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;       /* always 0 for Modbus */
    uint16_t length;            /* remaining bytes: unit_id + PDU */
    uint8_t  unit_id;
} modbus_tcp_header_t;

/** Parsed frame --- header + PDU pointer (zero-copy into input buffer). */
typedef struct {
    modbus_tcp_header_t header;
    uint8_t             function_code;
    const uint8_t      *data;       /* points into input buffer */
    uint16_t            data_len;
} modbus_tcp_frame_t;

/* ---------- build functions ---------- */

/**
 * @brief Build a Read Holding Registers request (FC 0x03).
 * @return Frame length on success, negative error code on failure.
 * @thread_safe: yes (pure function)
 */
int modbus_tcp_build_read_regs(uint8_t *buf, uint16_t buf_size,
                               uint16_t transaction_id, uint8_t unit_id,
                               uint16_t start_addr, uint16_t count);

/**
 * @brief Build a Write Single Register request (FC 0x06).
 * @return Frame length on success, negative error code on failure.
 * @thread_safe: yes (pure function)
 */
int modbus_tcp_build_write_reg(uint8_t *buf, uint16_t buf_size,
                               uint16_t transaction_id, uint8_t unit_id,
                               uint16_t addr, uint16_t value);

/**
 * @brief Build a Write Multiple Registers request (FC 0x10).
 * @return Frame length on success, negative error code on failure.
 * @thread_safe: yes (pure function)
 */
int modbus_tcp_build_write_regs(uint8_t *buf, uint16_t buf_size,
                                uint16_t transaction_id, uint8_t unit_id,
                                uint16_t start_addr, const uint16_t *values,
                                uint16_t count);

/* ---------- parse ---------- */

/**
 * @brief Parse a Modbus TCP frame from raw bytes.
 *
 * Validates MBAP header (protocol_id == 0, length field consistency).
 * On success, frame->data points into the input buffer (zero-copy).
 *
 * @return MODBUS_TCP_OK on success, negative error code on failure.
 * @thread_safe: yes (pure function)
 */
modbus_tcp_err_t modbus_tcp_parse(const uint8_t *buf, uint16_t len,
                                  modbus_tcp_frame_t *frame);

/* ---------- response helpers ---------- */

/**
 * @brief Extract register values from a parsed FC 0x03 read response.
 *
 * Converts big-endian register pairs from the PDU to host uint16_t.
 *
 * @param frame        Parsed response frame.
 * @param values       Output array.
 * @param max_count    Size of values array.
 * @param actual_count Output: number of registers extracted (may be NULL).
 * @return MODBUS_TCP_OK on success, negative error code on failure.
 * @thread_safe: yes (pure function)
 */
modbus_tcp_err_t modbus_tcp_get_regs(const modbus_tcp_frame_t *frame,
                                     uint16_t *values, uint16_t max_count,
                                     uint16_t *actual_count);

/**
 * @brief Check if a parsed frame is an exception response (bit 7 set).
 * @thread_safe: yes
 */
static inline bool modbus_tcp_is_exception(const modbus_tcp_frame_t *f)
{
    return f && (f->function_code & 0x80) != 0;
}

/**
 * @brief Get exception code from an exception response.
 * @thread_safe: yes
 */
static inline uint8_t modbus_tcp_exception_code(const modbus_tcp_frame_t *f)
{
    return (f && f->data_len > 0 && f->data) ? f->data[0] : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TCP_H */
