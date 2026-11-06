// SPDX-License-Identifier: MIT
/*
 * lorawan_packet — LoRaWAN MAC layer packet encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless frame building and parsing for LoRaWAN Class A
 * uplink/downlink data frames and join requests.
 * Codec only — no AES encryption, no MIC computation.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef LORAWAN_PACKET_H
#define LORAWAN_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LORAWAN_OK           =  0,
    LORAWAN_ERR_PARAM   = -1,
    LORAWAN_ERR_SIZE     = -2,
    LORAWAN_ERR_MIC      = -3,
    LORAWAN_ERR_OVERFLOW = -4,
} lorawan_err_t;

typedef enum {
    LORAWAN_JOIN_REQUEST    = 0x00,
    LORAWAN_JOIN_ACCEPT     = 0x01,
    LORAWAN_UNCONFIRMED_UP  = 0x02,
    LORAWAN_UNCONFIRMED_DOWN = 0x03,
    LORAWAN_CONFIRMED_UP    = 0x04,
    LORAWAN_CONFIRMED_DOWN  = 0x05,
} lorawan_mtype_t;

typedef struct {
    lorawan_mtype_t mtype;
    uint32_t dev_addr;
    uint32_t fcnt;
    uint8_t  fport;
    bool     ack;
    bool     adr;
    const uint8_t *payload;
    uint16_t payload_len;
    uint8_t  mic[4];
} lorawan_frame_t;

/**
 * Build a MAC data frame (MHDR + FHDR + FPort + FRMPayload + MIC).
 * Does NOT encrypt payload or compute MIC — caller does crypto and
 * fills frame->mic before calling this.
 * frame->payload_len above 242, the largest FRMPayload of any region
 * with FOpts empty, returns LORAWAN_ERR_SIZE.
 * Returns total bytes written, or negative lorawan_err_t on error.
 * @thread_safe: yes (pure function)
 */
int lorawan_build_data(uint8_t *buf, uint16_t buf_size,
                       const lorawan_frame_t *frame);

/**
 * Parse a MAC data frame into frame struct.
 * Extracts MIC bytes but does NOT verify them.
 * @thread_safe: yes (pure function)
 */
lorawan_err_t lorawan_parse_data(const uint8_t *buf, uint16_t len,
                                 lorawan_frame_t *frame);

/**
 * Build a join request frame (MHDR + AppEUI + DevEUI + DevNonce).
 * Does NOT append MIC — caller computes and appends 4-byte MIC after.
 * Returns total bytes written (19), or negative lorawan_err_t on error.
 * @thread_safe: yes (pure function)
 */
int lorawan_build_join_request(uint8_t *buf, uint16_t buf_size,
                               const uint8_t *app_eui,
                               const uint8_t *dev_eui,
                               const uint8_t *dev_nonce);

/**
 * Parse a join request frame.
 * @thread_safe: yes (pure function)
 */
lorawan_err_t lorawan_parse_join_request(const uint8_t *buf, uint16_t len,
                                         uint8_t *app_eui, uint8_t *dev_eui,
                                         uint8_t *dev_nonce);

/** Construct MHDR byte: (MType << 5) | (RFU << 2) | Major. @thread_safe: yes */
uint8_t lorawan_mhdr(lorawan_mtype_t mtype, uint8_t major);

/** Extract MType from MHDR byte. @thread_safe: yes */
lorawan_mtype_t lorawan_get_mtype(uint8_t mhdr);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_PACKET_H */
