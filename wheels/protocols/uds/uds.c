// SPDX-License-Identifier: MIT
/*
 * uds — Unified Diagnostic Services (ISO 14229) message codec
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "uds.h"
#include <string.h>

bool uds_is_positive_response(uint8_t sid_byte)
{
    return (sid_byte & 0x40) != 0 && sid_byte != UDS_SID_NEGATIVE_RESPONSE;
}

uds_err_t uds_decode(const uint8_t *buf, size_t len, uds_msg_t *out)
{
    if (!buf || !out)
        return UDS_ERR_PARAM;
    if (len < 1)
        return UDS_ERR_SHORT;

    memset(out, 0, sizeof(*out));

    uint8_t first = buf[0];

    if (first == UDS_SID_NEGATIVE_RESPONSE) {
        if (len < 3)
            return UDS_ERR_SHORT;
        out->sid = first;
        out->is_negative = true;
        out->is_response = true;
        out->rejected_sid = buf[1];
        out->nrc = buf[2];
        if (len > 3) {
            size_t extra = len - 3;
            if (extra > UDS_MAX_DATA)
                return UDS_ERR_OVERFLOW;
            memcpy(out->data, &buf[3], extra);
            out->data_len = (uint16_t)extra;
        }
        return UDS_OK;
    }

    if (uds_is_positive_response(first)) {
        out->sid = (uint8_t)(first & ~0x40u);
        out->is_response = true;
    } else {
        out->sid = first;
        out->is_response = false;
    }

    size_t payload = len - 1;
    if (payload > UDS_MAX_DATA)
        return UDS_ERR_OVERFLOW;
    if (payload > 0)
        memcpy(out->data, &buf[1], payload);
    out->data_len = (uint16_t)payload;

    return UDS_OK;
}

uds_err_t uds_encode_request(uint8_t sid, const uint8_t *data, size_t data_len,
                              uint8_t *buf, size_t cap, size_t *out_len)
{
    if (!buf || !out_len)
        return UDS_ERR_PARAM;
    if (data_len > 0 && !data)
        return UDS_ERR_PARAM;

    size_t total = 1 + data_len;
    if (total > cap)
        return UDS_ERR_OVERFLOW;

    buf[0] = sid;
    if (data_len > 0)
        memcpy(&buf[1], data, data_len);
    *out_len = total;
    return UDS_OK;
}

uds_err_t uds_encode_response(uint8_t sid, const uint8_t *data, size_t data_len,
                               uint8_t *buf, size_t cap, size_t *out_len)
{
    if (!buf || !out_len)
        return UDS_ERR_PARAM;
    if (data_len > 0 && !data)
        return UDS_ERR_PARAM;

    size_t total = 1 + data_len;
    if (total > cap)
        return UDS_ERR_OVERFLOW;

    buf[0] = (uint8_t)(sid | 0x40u);
    if (data_len > 0)
        memcpy(&buf[1], data, data_len);
    *out_len = total;
    return UDS_OK;
}

uds_err_t uds_encode_negative(uint8_t rejected_sid, uint8_t nrc,
                               uint8_t *buf, size_t cap, size_t *out_len)
{
    if (!buf || !out_len)
        return UDS_ERR_PARAM;
    if (cap < 3)
        return UDS_ERR_OVERFLOW;
    buf[0] = UDS_SID_NEGATIVE_RESPONSE;
    buf[1] = rejected_sid;
    buf[2] = nrc;
    *out_len = 3;
    return UDS_OK;
}
