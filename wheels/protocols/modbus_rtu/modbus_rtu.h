// SPDX-License-Identifier: MIT
/*
 * modbus_rtu — Modbus RTU frame codec with master/slave helpers
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      crc16
 * Instances: multi
 */

#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- compile-time feature selection ---------- */

#ifndef MODBUS_RTU_MASTER
#define MODBUS_RTU_MASTER  1
#endif

#ifndef MODBUS_RTU_SLAVE
#define MODBUS_RTU_SLAVE   1
#endif

/* ---------- constants ---------- */

#define MODBUS_FC_READ_HOLDING_REGS   0x03
#define MODBUS_FC_READ_INPUT_REGS     0x04
#define MODBUS_FC_WRITE_SINGLE_REG    0x06
#define MODBUS_FC_WRITE_MULTIPLE_REGS 0x10

#define MODBUS_EX_ILLEGAL_FUNCTION    0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDR   0x02
#define MODBUS_EX_ILLEGAL_DATA_VALUE  0x03
#define MODBUS_EX_SLAVE_FAILURE       0x04

#define MODBUS_RTU_MIN_FRAME      4
#define MODBUS_RTU_MAX_FRAME    256
#define MODBUS_RTU_ADDR_BROADCAST 0

/* Named return codes. Functions still return int because parse / build /
 * master / slave APIs mix length (>=0) and error code (<0) — same pattern
 * as fal partition I/O. */
typedef enum {
    MODBUS_RTU_OK         =  0,
    MODBUS_RTU_ERR_PARAM  = -1, /* NULL buf, bad length, invalid arg, bad CRC */
} modbus_rtu_err_t;

#ifndef MODBUS_MAX_READ_REGS
#define MODBUS_MAX_READ_REGS  125
#endif
#ifndef MODBUS_MAX_WRITE_REGS
#define MODBUS_MAX_WRITE_REGS 123
#endif

/* ---------- shared types ---------- */

/**
 * Parsed frame — data points into the original buffer (zero-copy).
 */
typedef struct {
    uint8_t        addr;
    uint8_t        func;
    const uint8_t *data;
    uint16_t       data_len;
} modbus_rtu_frame_t;

/* ---------- frame layer ---------- */

/**
 * @brief Validate CRC of a complete frame.
 * @thread_safe: yes
 */
bool modbus_rtu_check_crc(const uint8_t *frame, uint16_t len);

/**
 * @brief Parse raw bytes into a frame structure.
 *
 * Validates minimum length and CRC. On success, frame->data points
 * into the original buffer.
 *
 * @return 0 on success, -1 on error (too short, bad CRC).
 * @thread_safe: yes
 */
int modbus_rtu_parse(const uint8_t *buf, uint16_t len,
                     modbus_rtu_frame_t *frame);

/**
 * @brief Build a raw frame: [addr][func][data...][CRC_lo][CRC_hi].
 * @return Frame length on success, -1 if buffer too small.
 * @thread_safe: yes
 */
int modbus_rtu_build(uint8_t *buf, uint16_t buf_size,
                     uint8_t addr, uint8_t func,
                     const uint8_t *data, uint16_t data_len);

/**
 * @brief Check if a parsed frame is an exception response (bit 7 set).
 * @thread_safe: yes
 */
static inline bool modbus_rtu_is_exception(const modbus_rtu_frame_t *f)
{
    return f && (f->func & 0x80) != 0;
}

/**
 * @brief Get exception code from an exception response.
 * @thread_safe: yes
 */
static inline uint8_t modbus_rtu_exception_code(const modbus_rtu_frame_t *f)
{
    return (f && f->data_len > 0) ? f->data[0] : 0;
}

/* ---------- master helpers ---------- */

#if MODBUS_RTU_MASTER

/**
 * @brief Build a read registers request (FC 0x03 or 0x04).
 * @param func  MODBUS_FC_READ_HOLDING_REGS or MODBUS_FC_READ_INPUT_REGS.
 * @return Frame length, -1 on error.
 * @thread_safe: yes
 */
int modbus_rtu_master_read_regs(uint8_t *buf, uint16_t buf_size,
                                uint8_t slave, uint8_t func,
                                uint16_t start, uint16_t count);

/**
 * @brief Build a write single register request (FC 0x06).
 * @return Frame length, -1 on error.
 * @thread_safe: yes
 */
int modbus_rtu_master_write_reg(uint8_t *buf, uint16_t buf_size,
                                uint8_t slave,
                                uint16_t reg, uint16_t value);

/**
 * @brief Build a write multiple registers request (FC 0x10).
 * @return Frame length, -1 on error.
 * @thread_safe: yes
 */
int modbus_rtu_master_write_regs(uint8_t *buf, uint16_t buf_size,
                                 uint8_t slave,
                                 uint16_t start, uint16_t count,
                                 const uint16_t *values);

/**
 * @brief Extract register values from a FC 0x03/0x04 read response.
 *
 * Converts big-endian register pairs to host uint16_t.
 *
 * @param frame      Parsed response frame (already CRC-validated).
 * @param regs       Output array.
 * @param max_count  Size of regs array.
 * @return Number of registers extracted, -1 on error.
 * @thread_safe: yes
 */
int modbus_rtu_master_get_regs(const modbus_rtu_frame_t *frame,
                               uint16_t *regs, uint16_t max_count);

#endif /* MODBUS_RTU_MASTER */

/* ---------- slave ---------- */

#if MODBUS_RTU_SLAVE

/**
 * @brief Read a single register. Return 0 on success, -1 on error.
 */
typedef int (*modbus_rtu_reg_read_t)(void *ctx, uint16_t addr, uint16_t *value);

/**
 * @brief Write a single register. Return 0 on success, -1 on error.
 */
typedef int (*modbus_rtu_reg_write_t)(void *ctx, uint16_t addr, uint16_t value);

typedef struct {
    uint8_t                addr;
    modbus_rtu_reg_read_t  read_holding;
    modbus_rtu_reg_read_t  read_input;
    modbus_rtu_reg_write_t write_holding;
    void                  *ctx;
} modbus_rtu_slave_t;

/**
 * @brief Initialize slave context.
 * @thread_safe: no
 */
void modbus_rtu_slave_init(modbus_rtu_slave_t *s, uint8_t addr, void *ctx);

/**
 * @brief Process a received request and build response.
 *
 * Checks address match, validates request, dispatches to register
 * callbacks, and builds the response frame (normal or exception).
 * Broadcast (addr 0) executes writes silently, skips reads.
 *
 * @return Response length (>0), 0 if not for us or broadcast, -1 on error.
 * @thread_safe: caller_must_lock
 */
int modbus_rtu_slave_process(modbus_rtu_slave_t *s,
                             const uint8_t *req, uint16_t req_len,
                             uint8_t *resp, uint16_t resp_size);

#endif /* MODBUS_RTU_SLAVE */

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H */
