// SPDX-License-Identifier: MIT
/*
 * tftp — TFTP protocol codec (RFC 1350)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless TFTP packet building and parsing. Handles RRQ/WRQ/DATA/ACK/ERROR
 * opcodes. Pure C99, no heap. All buffers caller-provided.
 */

#ifndef TFTP_H
#define TFTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define TFTP_BLOCK_SIZE    512
#define TFTP_MAX_FILENAME  128

/* ---------- error codes (return values) ---------- */

typedef enum {
    TFTP_OK           =  0,
    TFTP_ERR_PARAM    = -1,
    TFTP_ERR_FORMAT   = -2,
    TFTP_ERR_SHORT    = -3,
    TFTP_ERR_OVERFLOW = -4,
} tftp_err_t;

/* ---------- TFTP opcodes ---------- */

typedef enum {
    TFTP_OP_RRQ   = 1,   /* Read request */
    TFTP_OP_WRQ   = 2,   /* Write request */
    TFTP_OP_DATA  = 3,   /* Data */
    TFTP_OP_ACK   = 4,   /* Acknowledgment */
    TFTP_OP_ERROR = 5,   /* Error */
} tftp_op_t;

/* ---------- TFTP error codes (in ERROR packets) ---------- */

typedef enum {
    TFTP_ERRC_UNDEFINED     = 0,
    TFTP_ERRC_NOT_FOUND     = 1,
    TFTP_ERRC_ACCESS        = 2,
    TFTP_ERRC_DISK_FULL     = 3,
    TFTP_ERRC_ILLEGAL_OP    = 4,
    TFTP_ERRC_UNKNOWN_ID    = 5,
    TFTP_ERRC_FILE_EXISTS   = 6,
    TFTP_ERRC_NO_SUCH_USER  = 7,
} tftp_error_code_t;

/* ---------- parsed packet ---------- */

typedef struct {
    uint16_t opcode;
    union {
        struct {
            char filename[TFTP_MAX_FILENAME];
            char mode[16];
        } rq;                                        /* RRQ / WRQ */
        struct {
            uint16_t       block;
            const uint8_t *data;
            size_t         len;
        } data;                                      /* DATA */
        struct {
            uint16_t block;
        } ack;                                       /* ACK */
        struct {
            uint16_t code;
            char     msg[128];
        } error;                                     /* ERROR */
    } u;
} tftp_packet_t;

/* ---------- API ---------- */

/**
 * Parse a TFTP packet from raw bytes.
 * Returns 0 on success, negative tftp_err_t on failure. A request naming a
 * file of TFTP_MAX_FILENAME bytes or more returns TFTP_ERR_OVERFLOW, and so
 * does a DATA packet of more than TFTP_BLOCK_SIZE bytes of data.
 * @thread_safe: yes (pure function)
 */
int tftp_parse(const uint8_t *buf, size_t len, tftp_packet_t *pkt);

/**
 * Build a RRQ (Read Request) packet.
 * Returns bytes written on success, negative tftp_err_t on failure. A filename
 * of TFTP_MAX_FILENAME bytes or more returns TFTP_ERR_OVERFLOW.
 * @thread_safe: yes (pure function)
 */
int tftp_build_rrq(uint8_t *buf, size_t cap,
                   const char *filename, const char *mode);

/**
 * Build a WRQ (Write Request) packet.
 * Returns bytes written on success, negative tftp_err_t on failure. A filename
 * of TFTP_MAX_FILENAME bytes or more returns TFTP_ERR_OVERFLOW.
 * @thread_safe: yes (pure function)
 */
int tftp_build_wrq(uint8_t *buf, size_t cap,
                   const char *filename, const char *mode);

/**
 * Build a DATA packet.
 * Returns bytes written on success, negative tftp_err_t on failure.
 * @thread_safe: yes (pure function)
 */
int tftp_build_data(uint8_t *buf, size_t cap,
                    uint16_t block, const uint8_t *data, size_t len);

/**
 * Build an ACK packet.
 * Returns bytes written on success, negative tftp_err_t on failure.
 * @thread_safe: yes (pure function)
 */
int tftp_build_ack(uint8_t *buf, size_t cap, uint16_t block);

/**
 * Build an ERROR packet.
 * Returns bytes written on success, negative tftp_err_t on failure.
 * @thread_safe: yes (pure function)
 */
int tftp_build_error(uint8_t *buf, size_t cap,
                     uint16_t code, const char *msg);

/**
 * Check if a parsed DATA packet is the last block (len < TFTP_BLOCK_SIZE).
 * @thread_safe: yes (pure function)
 */
bool tftp_is_last_data(const tftp_packet_t *pkt);

#ifdef __cplusplus
}
#endif

#endif /* TFTP_H */
