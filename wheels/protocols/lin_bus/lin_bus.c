// SPDX-License-Identifier: MIT
/*
 * lin_bus -- LIN bus protocol frame encoding/decoding
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "lin_bus.h"
#include "contract.h"
#include <string.h>

/* ---------- PID calculation ---------- */

uint8_t lin_bus_calc_pid(uint8_t id)
{
    if (id > LIN_BUS_MAX_ID) return 0;

    uint8_t id0 = (uint8_t)((id >> 0) & 1u);
    uint8_t id1 = (uint8_t)((id >> 1) & 1u);
    uint8_t id2 = (uint8_t)((id >> 2) & 1u);
    uint8_t id3 = (uint8_t)((id >> 3) & 1u);
    uint8_t id4 = (uint8_t)((id >> 4) & 1u);
    uint8_t id5 = (uint8_t)((id >> 5) & 1u);

    /* P0 = ID0 ^ ID1 ^ ID2 ^ ID4 */
    uint8_t p0 = (uint8_t)(id0 ^ id1 ^ id2 ^ id4);
    /* P1 = ~(ID1 ^ ID3 ^ ID4 ^ ID5) */
    uint8_t p1 = (uint8_t)(~(id1 ^ id3 ^ id4 ^ id5) & 1u);

    return (uint8_t)(id | (uint8_t)(p0 << 6) | (uint8_t)(p1 << 7));
}

bool lin_bus_verify_pid(uint8_t pid)
{
    uint8_t id = (uint8_t)(pid & 0x3Fu);
    uint8_t expected = lin_bus_calc_pid(id);
    return pid == expected;
}

/* ---------- checksum ---------- */

uint8_t lin_bus_calc_checksum(const lin_bus_frame_t *frame)
{
    if (!frame) return 0;
    if (frame->data_len == 0 || frame->data_len > LIN_BUS_MAX_DATA_LEN) return 0;

    uint16_t sum = 0;

    /* Diagnostic frames (ID 60, 61) always use classic checksum per LIN spec,
     * regardless of the checksum_type field. */
    if (frame->checksum_type == LIN_BUS_CHECKSUM_ENHANCED &&
        frame->id != LIN_BUS_ID_DIAG_REQ &&
        frame->id != LIN_BUS_ID_DIAG_RESP) {
        sum = lin_bus_calc_pid(frame->id);
    }

    for (size_t i = 0; i < frame->data_len; i++) {
        sum = (uint16_t)(sum + frame->data[i]);
        if (sum > 0xFFu) {
            sum = (uint16_t)(sum - 0xFFu);  /* carry wraparound */
        }
    }

    return (uint8_t)(~sum & 0xFFu);
}

/* ---------- encode ---------- */

lin_bus_err_t lin_bus_encode(const lin_bus_frame_t *frame,
                             uint8_t *buf, size_t cap, size_t *len)
{
    if (!frame || !buf || !len) return LIN_BUS_ERR_PARAM;
    if (frame->id > LIN_BUS_MAX_ID) return LIN_BUS_ERR_PARAM;
    if (frame->data_len == 0 || frame->data_len > LIN_BUS_MAX_DATA_LEN)
        return LIN_BUS_ERR_PARAM;

    /* Frame: sync(1) + PID(1) + data(data_len) + checksum(1) */
    size_t required = (size_t)3 + frame->data_len;
    if (cap < required) return LIN_BUS_ERR_PARAM;

    size_t pos = 0;

    /* Sync byte */
    buf[pos++] = LIN_BUS_SYNC_BYTE;

    /* PID */
    buf[pos++] = lin_bus_calc_pid(frame->id);

    /* Data */
    for (size_t i = 0; i < frame->data_len; i++) {
        buf[pos++] = frame->data[i];
    }

    /* Checksum */
    buf[pos++] = lin_bus_calc_checksum(frame);

    *len = pos;
    ENSURE(pos == required);
    ENSURE(pos <= cap);
    return LIN_BUS_OK;
}

/* ---------- decode ---------- */

lin_bus_err_t lin_bus_decode(const uint8_t *buf, size_t len,
                             lin_bus_frame_t *frame)
{
    if (!buf || !frame) return LIN_BUS_ERR_PARAM;

    /* Minimum: sync(1) + PID(1) + data(1) + checksum(1) = 4 */
    if (len < 4) return LIN_BUS_ERR_FORMAT;

    /* Maximum: sync(1) + PID(1) + data(8) + checksum(1) = 11 */
    if (len > LIN_BUS_MAX_FRAME_LEN) return LIN_BUS_ERR_FORMAT;

    /* Verify sync byte */
    if (buf[0] != LIN_BUS_SYNC_BYTE) return LIN_BUS_ERR_SYNC;

    /* Verify PID parity */
    uint8_t pid = buf[1];
    if (!lin_bus_verify_pid(pid)) return LIN_BUS_ERR_PARITY;

    uint8_t id = (uint8_t)(pid & 0x3Fu);
    uint8_t data_len = (uint8_t)(len - 3); /* subtract sync + PID + checksum */

    if (data_len == 0 || data_len > LIN_BUS_MAX_DATA_LEN)
        return LIN_BUS_ERR_FORMAT;

    /* Fill frame struct -- try both checksum types to find which matches */
    memset(frame, 0, sizeof(*frame));
    frame->id       = id;
    frame->data_len = data_len;
    memcpy(frame->data, &buf[2], data_len);

    uint8_t rx_checksum = buf[len - 1];

    /* Diagnostic frames (ID 60, 61) always use classic checksum per LIN spec */
    if (id == LIN_BUS_ID_DIAG_REQ || id == LIN_BUS_ID_DIAG_RESP) {
        frame->checksum_type = LIN_BUS_CHECKSUM_CLASSIC;
        uint8_t expected = lin_bus_calc_checksum(frame);
        if (expected == rx_checksum) return LIN_BUS_OK;
        return LIN_BUS_ERR_CHECKSUM;
    }

    /* Non-diagnostic: try enhanced first (LIN 2.0+), then classic */
    frame->checksum_type = LIN_BUS_CHECKSUM_ENHANCED;
    uint8_t expected = lin_bus_calc_checksum(frame);
    if (expected == rx_checksum) return LIN_BUS_OK;

    frame->checksum_type = LIN_BUS_CHECKSUM_CLASSIC;
    expected = lin_bus_calc_checksum(frame);
    if (expected == rx_checksum) return LIN_BUS_OK;

    return LIN_BUS_ERR_CHECKSUM;
}
