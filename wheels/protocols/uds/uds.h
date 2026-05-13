// SPDX-License-Identifier: MIT
/*
 * uds — Unified Diagnostic Services (ISO 14229) message codec
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Encode/decode UDS request and response messages. Supports common
 * services: DiagnosticSessionControl, ECUReset, ReadDataByIdentifier,
 * WriteDataByIdentifier, NegativeResponse. Transport-independent
 * (pair with ISO-TP for CAN). Pure C99, no heap, no OS.
 */

#ifndef UDS_H
#define UDS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDS_MAX_DATA 256

/* Common SIDs */
#define UDS_SID_DIAG_SESSION_CTRL   0x10
#define UDS_SID_ECU_RESET           0x11
#define UDS_SID_READ_DATA_BY_ID     0x22
#define UDS_SID_WRITE_DATA_BY_ID    0x2E
#define UDS_SID_TESTER_PRESENT      0x3E
#define UDS_SID_NEGATIVE_RESPONSE   0x7F

/* Common NRC codes */
#define UDS_NRC_GENERAL_REJECT           0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED    0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12
#define UDS_NRC_CONDITIONS_NOT_CORRECT   0x22
#define UDS_NRC_REQUEST_OUT_OF_RANGE     0x31
#define UDS_NRC_RESPONSE_PENDING         0x78

typedef enum {
    UDS_OK           =  0,
    UDS_ERR_PARAM    = -1,
    UDS_ERR_FORMAT   = -2,
    UDS_ERR_SHORT    = -3,
    UDS_ERR_OVERFLOW = -4,
} uds_err_t;

typedef struct {
    uint8_t sid;
    uint8_t data[UDS_MAX_DATA];
    uint16_t data_len;
    bool    is_response;
    bool    is_negative;
    uint8_t nrc;
    uint8_t rejected_sid;
} uds_msg_t;

/** Decode a UDS message from raw bytes. @thread_safe: yes @isr_safe: yes */
uds_err_t uds_decode(const uint8_t *buf, size_t len, uds_msg_t *out);

/** Encode a UDS request (SID + data). @thread_safe: yes @isr_safe: yes */
uds_err_t uds_encode_request(uint8_t sid, const uint8_t *data, size_t data_len,
                              uint8_t *buf, size_t cap, size_t *out_len);

/** Encode a UDS positive response (SID+0x40 + data). @thread_safe: yes @isr_safe: yes */
uds_err_t uds_encode_response(uint8_t sid, const uint8_t *data, size_t data_len,
                               uint8_t *buf, size_t cap, size_t *out_len);

/** Encode a negative response. @thread_safe: yes @isr_safe: yes */
uds_err_t uds_encode_negative(uint8_t rejected_sid, uint8_t nrc,
                               uint8_t *buf, size_t cap, size_t *out_len);

/** Check if SID byte indicates a positive response. @thread_safe: yes @isr_safe: yes */
bool uds_is_positive_response(uint8_t sid_byte);

#ifdef __cplusplus
}
#endif

#endif /* UDS_H */
