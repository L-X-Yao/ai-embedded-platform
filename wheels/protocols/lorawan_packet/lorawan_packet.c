/*
 * lorawan_packet — LoRaWAN MAC layer packet encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "lorawan_packet.h"
#include <string.h>

/* LoRaWAN wire format is little-endian for DevAddr and FCnt */

static void put_le32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8)  & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static uint32_t get_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static void put_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static uint16_t get_le16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

uint8_t lorawan_mhdr(lorawan_mtype_t mtype, uint8_t major)
{
    return (uint8_t)(((uint8_t)mtype << 5) | (major & 0x03));
}

lorawan_mtype_t lorawan_get_mtype(uint8_t mhdr)
{
    return (lorawan_mtype_t)(mhdr >> 5);
}

int lorawan_build_data(uint8_t *buf, uint16_t buf_size,
                       const lorawan_frame_t *frame)
{
    if (!buf || !frame) return LORAWAN_ERR_PARAMS;
    if (frame->payload_len > 0 && !frame->payload) return LORAWAN_ERR_PARAMS;

    /*
     * Frame layout:
     *   MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + FOpts(0)
     *   + [FPort(1) + FRMPayload(N)] + MIC(4)
     *
     * FPort is present only when payload_len > 0.
     */
    bool has_payload = (frame->payload_len > 0);
    uint16_t total = (uint16_t)(1 + 4 + 1 + 2
                     + (has_payload ? 1 + frame->payload_len : 0)
                     + 4);

    if (total > buf_size) return LORAWAN_ERR_OVERFLOW;

    int pos = 0;

    /* MHDR */
    buf[pos++] = lorawan_mhdr(frame->mtype, 0x00);

    /* DevAddr — little-endian */
    put_le32(buf + pos, frame->dev_addr);
    pos += 4;

    /* FCtrl: ADR(1) | ADRACKReq(1) | ACK(1) | ClassB/FPending(1) | FOptsLen(4) */
    uint8_t fctrl = 0;
    if (frame->adr) fctrl |= 0x80;
    if (frame->ack) fctrl |= 0x20;
    /* FOptsLen = 0, ClassB/FPending = 0 */
    buf[pos++] = fctrl;

    /* FCnt — lower 16 bits, little-endian */
    put_le16(buf + pos, (uint16_t)(frame->fcnt & 0xFFFF));
    pos += 2;

    /* FPort + FRMPayload (only if payload present) */
    if (has_payload)
    {
        buf[pos++] = frame->fport;
        memcpy(buf + pos, frame->payload, frame->payload_len);
        pos += frame->payload_len;
    }

    /* MIC — caller-provided, copied verbatim */
    memcpy(buf + pos, frame->mic, 4);
    pos += 4;

    return pos;
}

lorawan_err_t lorawan_parse_data(const uint8_t *buf, uint16_t len,
                                 lorawan_frame_t *frame)
{
    if (!buf || !frame) return LORAWAN_ERR_PARAMS;

    /* Minimum: MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + MIC(4) = 12 */
    if (len < 12) return LORAWAN_ERR_SIZE;

    int pos = 0;

    /* MHDR */
    frame->mtype = lorawan_get_mtype(buf[pos]);
    pos++;

    /* DevAddr — little-endian */
    frame->dev_addr = get_le32(buf + pos);
    pos += 4;

    /* FCtrl */
    uint8_t fctrl = buf[pos++];
    frame->adr = (fctrl & 0x80) != 0;
    frame->ack = (fctrl & 0x20) != 0;
    uint8_t fopts_len = fctrl & 0x0F;

    /* FCnt — lower 16 bits */
    frame->fcnt = get_le16(buf + pos);
    pos += 2;

    /* Skip FOpts if present */
    if (pos + fopts_len > len - 4) return LORAWAN_ERR_SIZE;
    pos += fopts_len;

    /* MIC is the last 4 bytes */
    memcpy(frame->mic, buf + len - 4, 4);

    /* FPort + FRMPayload: everything between FOpts and MIC */
    uint16_t remaining = (uint16_t)(len - 4 - (uint16_t)pos);
    if (remaining > 0)
    {
        frame->fport = buf[pos++];
        remaining--;
        frame->payload = (remaining > 0) ? (buf + pos) : NULL;
        frame->payload_len = (uint8_t)remaining;
    }
    else
    {
        frame->fport = 0;
        frame->payload = NULL;
        frame->payload_len = 0;
    }

    return LORAWAN_OK;
}

int lorawan_build_join_request(uint8_t *buf, uint16_t buf_size,
                               const uint8_t *app_eui,
                               const uint8_t *dev_eui,
                               const uint8_t *dev_nonce)
{
    if (!buf || !app_eui || !dev_eui || !dev_nonce) return LORAWAN_ERR_PARAMS;

    /* Join request: MHDR(1) + AppEUI(8) + DevEUI(8) + DevNonce(2) = 19 */
    if (buf_size < 19) return LORAWAN_ERR_OVERFLOW;

    int pos = 0;

    /* MHDR */
    buf[pos++] = lorawan_mhdr(LORAWAN_JOIN_REQUEST, 0x00);

    /* AppEUI — 8 bytes as-is (LSB first per spec) */
    memcpy(buf + pos, app_eui, 8);
    pos += 8;

    /* DevEUI — 8 bytes as-is (LSB first per spec) */
    memcpy(buf + pos, dev_eui, 8);
    pos += 8;

    /* DevNonce — 2 bytes as-is */
    memcpy(buf + pos, dev_nonce, 2);
    pos += 2;

    return pos;
}

lorawan_err_t lorawan_parse_join_request(const uint8_t *buf, uint16_t len,
                                         uint8_t *app_eui, uint8_t *dev_eui,
                                         uint8_t *dev_nonce)
{
    if (!buf || !app_eui || !dev_eui || !dev_nonce) return LORAWAN_ERR_PARAMS;

    /* Minimum 19 bytes: MHDR(1) + AppEUI(8) + DevEUI(8) + DevNonce(2) */
    if (len < 19) return LORAWAN_ERR_SIZE;

    /* Verify MType */
    if (lorawan_get_mtype(buf[0]) != LORAWAN_JOIN_REQUEST) return LORAWAN_ERR_PARAMS;

    int pos = 1;

    memcpy(app_eui, buf + pos, 8);
    pos += 8;

    memcpy(dev_eui, buf + pos, 8);
    pos += 8;

    memcpy(dev_nonce, buf + pos, 2);

    return LORAWAN_OK;
}
