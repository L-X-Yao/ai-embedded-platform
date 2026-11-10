// SPDX-License-Identifier: MIT
/*
 * mqtt5_packet — MQTT 5.0 packet encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "mqtt5_packet.h"
#include "contract.h"
#include <string.h>

/* ---- internal helpers -------------------------------------------------- */

static int put_u8(uint8_t *buf, uint8_t val)
{
    buf[0] = val;
    return 1;
}

static int put_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
    return 2;
}

static int put_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val);
    return 4;
}

static uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

static uint32_t get_u32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
}

static int put_str(uint8_t *buf, const char *s)
{
    size_t slen = strlen(s);
    if (slen > UINT16_MAX) return -1;
    put_u16(buf, (uint16_t)slen);
    memcpy(buf + 2, s, slen);
    return 2 + (int)slen;
}

/* ---- VBI (variable byte integer) --------------------------------------- */

int mqtt5_encode_vbi(uint8_t *buf, uint32_t value)
{
    if (!buf) return MQTT5_ERR_PARAM;
    if (value > 268435455u) return MQTT5_ERR_OVERFLOW;

    int i = 0;
    do {
        uint8_t byte = (uint8_t)(value % 128u);
        value /= 128u;
        if (value > 0) byte |= 0x80;
        buf[i++] = byte;
    } while (value > 0);
    return i;
}

mqtt5_err_t mqtt5_decode_vbi(const uint8_t *buf, uint16_t len,
                             uint32_t *value, uint8_t *bytes_used)
{
    if (!buf || !value || !bytes_used) return MQTT5_ERR_PARAM;
    if (len == 0) return MQTT5_ERR_SIZE;

    uint32_t result = 0;
    uint32_t multiplier = 1;
    uint8_t i = 0;

    do {
        if (i >= len) return MQTT5_ERR_SIZE;
        result += (uint32_t)(buf[i] & 0x7F) * multiplier;
        if (multiplier >= 128u * 128u * 128u && (buf[i] & 0x80)) {
            return MQTT5_ERR_OVERFLOW;
        }
        multiplier *= 128u;
    } while ((buf[i++] & 0x80) != 0);

    *value = result;
    *bytes_used = i;
    return MQTT5_OK;
}

/* ---- builders ---------------------------------------------------------- */

int mqtt5_build_connect(uint8_t *buf, uint16_t size,
                        const mqtt5_connect_t *cfg)
{
    if (!buf || !cfg || !cfg->client_id) return MQTT5_ERR_PARAM;
    if (cfg->password && !cfg->username) return MQTT5_ERR_PARAM;

    /* compute properties length */
    uint16_t props_len = 0;
    if (cfg->session_expiry > 0) props_len += 5;  /* id(1) + u32(4) */

    /* variable header: protocol name(6) + version(1) + flags(1) + keep_alive(2) + props_vbi + props */
    uint8_t props_vbi[4];
    int props_vbi_len = mqtt5_encode_vbi(props_vbi, props_len);

    uint8_t flags = 0;
    if (cfg->clean_start) flags |= 0x02;
    if (cfg->username)    flags |= 0x80;
    if (cfg->password)    flags |= 0x40;

    size_t payload_len = 2 + strlen(cfg->client_id);
    if (cfg->username) payload_len += 2 + strlen(cfg->username);
    if (cfg->password) payload_len += 2 + strlen(cfg->password);

    /* remaining = var_header(10) + props_vbi + props + payload */
    uint32_t remaining = 10 + (uint32_t)props_vbi_len + props_len + (uint32_t)payload_len;
    uint8_t rl_buf[4];
    int rl_len = mqtt5_encode_vbi(rl_buf, remaining);
    if (rl_len < 0) return MQTT5_ERR_OVERFLOW;

    uint32_t total = 1 + (uint32_t)rl_len + remaining;
    if (total > size) return MQTT5_ERR_SIZE;

    int pos = 0;
    /* fixed header: CONNECT = 0x10 */
    buf[pos++] = 0x10;
    memcpy(buf + pos, rl_buf, (size_t)rl_len); pos += rl_len;

    /* variable header: protocol name */
    pos += put_u16(buf + pos, 0x0004);
    buf[pos++] = 'M'; buf[pos++] = 'Q'; buf[pos++] = 'T'; buf[pos++] = 'T';

    /* protocol version = 5 */
    buf[pos++] = 0x05;
    buf[pos++] = flags;
    pos += put_u16(buf + pos, cfg->keep_alive);

    /* properties */
    memcpy(buf + pos, props_vbi, (size_t)props_vbi_len); pos += props_vbi_len;
    if (cfg->session_expiry > 0) {
        pos += put_u8(buf + pos, MQTT5_PROP_SESSION_EXPIRY);
        pos += put_u32(buf + pos, cfg->session_expiry);
    }

    /* payload: client_id, username, password */
    int n = put_str(buf + pos, cfg->client_id);
    if (n < 0) return MQTT5_ERR_SIZE;
    pos += n;
    if (cfg->username) {
        n = put_str(buf + pos, cfg->username);
        if (n < 0) return MQTT5_ERR_SIZE;
        pos += n;
    }
    if (cfg->password) {
        n = put_str(buf + pos, cfg->password);
        if (n < 0) return MQTT5_ERR_SIZE;
        pos += n;
    }

    ENSURE((uint32_t)pos <= size);
    return pos;
}

int mqtt5_build_publish(uint8_t *buf, uint16_t size,
                        const mqtt5_publish_t *pub)
{
    if (!buf || !pub || !pub->topic) return MQTT5_ERR_PARAM;
    if (pub->qos > 2) return MQTT5_ERR_PARAM;
    if (!pub->payload && pub->payload_len > 0) return MQTT5_ERR_PARAM;

    size_t topic_raw = strlen(pub->topic);
    if (topic_raw > UINT16_MAX) return MQTT5_ERR_SIZE;
    uint16_t topic_len = (uint16_t)topic_raw;

    /* properties */
    uint16_t props_len = 0;
    if (pub->message_expiry > 0) props_len += 5;

    uint8_t props_vbi[4];
    int props_vbi_len = mqtt5_encode_vbi(props_vbi, props_len);

    uint16_t pkt_id_len = (pub->qos > 0) ? 2u : 0u;
    uint32_t remaining = 2u + topic_len + pkt_id_len +
                         (uint32_t)props_vbi_len + props_len +
                         pub->payload_len;

    uint8_t rl_buf[4];
    int rl_len = mqtt5_encode_vbi(rl_buf, remaining);
    if (rl_len < 0) return MQTT5_ERR_OVERFLOW;

    uint32_t total = 1 + (uint32_t)rl_len + remaining;
    if (total > size) return MQTT5_ERR_SIZE;

    uint8_t byte0 = 0x30; /* PUBLISH = 3 << 4 */
    byte0 = (uint8_t)(byte0 | (uint8_t)((pub->qos & 0x03) << 1));
    if (pub->retain) byte0 |= 0x01;

    int pos = 0;
    buf[pos++] = byte0;
    memcpy(buf + pos, rl_buf, (size_t)rl_len); pos += rl_len;
    pos += put_u16(buf + pos, topic_len);
    memcpy(buf + pos, pub->topic, topic_len); pos += (int)topic_len;
    if (pub->qos > 0) {
        pos += put_u16(buf + pos, pub->packet_id);
    }

    /* properties */
    memcpy(buf + pos, props_vbi, (size_t)props_vbi_len); pos += props_vbi_len;
    if (pub->message_expiry > 0) {
        pos += put_u8(buf + pos, MQTT5_PROP_MESSAGE_EXPIRY);
        pos += put_u32(buf + pos, pub->message_expiry);
    }

    /* payload */
    if (pub->payload && pub->payload_len > 0) {
        memcpy(buf + pos, pub->payload, pub->payload_len);
        pos += (int)pub->payload_len;
    }

    return pos;
}

int mqtt5_build_subscribe(uint8_t *buf, uint16_t size,
                          uint16_t packet_id, const char *topic,
                          uint8_t qos)
{
    if (!buf || !topic) return MQTT5_ERR_PARAM;
    if (qos > 2) return MQTT5_ERR_PARAM;

    size_t topic_raw = strlen(topic);
    if (topic_raw > UINT16_MAX) return MQTT5_ERR_SIZE;
    uint16_t topic_len = (uint16_t)topic_raw;

    /* SUBSCRIBE: packet_id(2) + props_len_vbi(1, =0) + topic_len(2) + topic + sub_opts(1) */
    uint32_t remaining = 2 + 1 + 2 + topic_len + 1;

    uint8_t rl_buf[4];
    int rl_len = mqtt5_encode_vbi(rl_buf, remaining);
    if (rl_len < 0) return MQTT5_ERR_OVERFLOW;

    uint32_t total = 1 + (uint32_t)rl_len + remaining;
    if (total > size) return MQTT5_ERR_SIZE;

    int pos = 0;
    buf[pos++] = 0x82; /* SUBSCRIBE = 8<<4 | 0x02 (reserved bits) */
    memcpy(buf + pos, rl_buf, (size_t)rl_len); pos += rl_len;
    pos += put_u16(buf + pos, packet_id);

    /* properties: none (length = 0) */
    buf[pos++] = 0x00;

    pos += put_u16(buf + pos, topic_len);
    memcpy(buf + pos, topic, topic_len); pos += (int)topic_len;

    /* subscription options byte: QoS in bits 0-1 */
    buf[pos++] = qos & 0x03;

    return pos;
}

int mqtt5_build_disconnect(uint8_t *buf, uint16_t size,
                           uint8_t reason_code)
{
    if (!buf) return MQTT5_ERR_PARAM;

    /* DISCONNECT with reason code 0x00 and no properties can be 2 bytes */
    if (reason_code == 0x00) {
        if (size < 2) return MQTT5_ERR_SIZE;
        buf[0] = 0xE0; /* DISCONNECT = 14<<4 */
        buf[1] = 0x00; /* remaining length = 0 */
        return 2;
    }

    /* with reason code: remaining = 1(reason) + 1(props_len=0) = 2 */
    if (size < 4) return MQTT5_ERR_SIZE;
    buf[0] = 0xE0;
    buf[1] = 0x02; /* remaining length = 2 */
    buf[2] = reason_code;
    buf[3] = 0x00; /* properties length = 0 */
    return 4;
}

/* ---- property skipping ------------------------------------------------- */

/*
 * The spec fixes the type of every property (MQTT 5.0 section 2.2.2.2), so one
 * with no field here can still be stepped over, and brokers send them by
 * default. An ID the spec does not allow in the packet stays an error: that
 * section calls it a Malformed Packet.
 */
typedef enum {
    PROP_INVALID,
    PROP_BYTE,
    PROP_U16,
    PROP_U32,
    PROP_VBI,
    PROP_STRING,    /* UTF-8 string or binary data: u16 length, then bytes */
    PROP_PAIR,      /* two strings */
} prop_kind_t;

static prop_kind_t connack_prop_kind(uint8_t id)
{
    switch (id) {
    case 0x25:  /* Retain Available */
    case 0x28:  /* Wildcard Subscription Available */
    case 0x29:  /* Subscription Identifier Available */
    case 0x2A:  /* Shared Subscription Available */
        return PROP_BYTE;
    case 0x13:  /* Server Keep Alive */
    case 0x22:  /* Topic Alias Maximum */
        return PROP_U16;
    case 0x27:  /* Maximum Packet Size */
        return PROP_U32;
    case 0x12:  /* Assigned Client Identifier */
    case 0x15:  /* Authentication Method */
    case 0x16:  /* Authentication Data */
    case 0x1A:  /* Response Information */
    case 0x1C:  /* Server Reference */
    case 0x1F:  /* Reason String */
        return PROP_STRING;
    case 0x26:  /* User Property */
        return PROP_PAIR;
    default:
        return PROP_INVALID;
    }
}

static prop_kind_t publish_prop_kind(uint8_t id)
{
    switch (id) {
    case 0x01:  /* Payload Format Indicator */
        return PROP_BYTE;
    case 0x23:  /* Topic Alias */
        return PROP_U16;
    case 0x0B:  /* Subscription Identifier */
        return PROP_VBI;
    case 0x03:  /* Content Type */
    case 0x08:  /* Response Topic */
    case 0x09:  /* Correlation Data */
        return PROP_STRING;
    case 0x26:  /* User Property */
        return PROP_PAIR;
    default:
        return PROP_INVALID;
    }
}

/* Step *pos over one property value, which has to end by end. */
static mqtt5_err_t skip_prop(prop_kind_t kind, const uint8_t *buf,
                             uint16_t *pos, uint32_t end)
{
    uint32_t next = *pos;
    uint32_t strings = 0;

    switch (kind) {
    case PROP_BYTE:
        next += 1;
        break;
    case PROP_U16:
        next += 2;
        break;
    case PROP_U32:
        next += 4;
        break;
    case PROP_VBI: {
        uint32_t value;
        uint8_t used;
        if (mqtt5_decode_vbi(buf + next, (uint16_t)(end - next),
                             &value, &used) != MQTT5_OK) {
            return MQTT5_ERR_PROTOCOL;
        }
        next += used;
        break;
    }
    case PROP_STRING:
        strings = 1;
        break;
    case PROP_PAIR:
        strings = 2;
        break;
    case PROP_INVALID:
    default:
        return MQTT5_ERR_PROTOCOL;
    }

    for (; strings > 0; strings--) {
        if (next + 2 > end) return MQTT5_ERR_PROTOCOL;
        next += 2u + get_u16(buf + next);
    }

    if (next > end) return MQTT5_ERR_PROTOCOL;
    *pos = (uint16_t)next;
    return MQTT5_OK;
}

/* ---- parsers ----------------------------------------------------------- */

mqtt5_err_t mqtt5_parse_connack(const uint8_t *buf, uint16_t len,
                                mqtt5_connack_t *ack)
{
    if (!buf || !ack) return MQTT5_ERR_PARAM;
    if (len < 5) return MQTT5_ERR_SIZE;
    if ((buf[0] >> 4) != 2) return MQTT5_ERR_PROTOCOL; /* not CONNACK */

    /* decode remaining length */
    uint32_t remaining;
    uint8_t rl_bytes;
    mqtt5_err_t err = mqtt5_decode_vbi(buf + 1, (uint16_t)(len - 1),
                                       &remaining, &rl_bytes);
    if (err != MQTT5_OK) return err;

    uint16_t pos = (uint16_t)(1 + rl_bytes);
    if (pos + 2 > len) return MQTT5_ERR_SIZE;

    /* acknowledge flags + reason code */
    ack->session_present = (buf[pos] & 0x01) != 0;
    ack->reason_code = buf[pos + 1];
    pos += 2;

    /* defaults for optional properties */
    ack->session_expiry = 0;
    ack->receive_max = 65535u;
    ack->max_qos = 2;

    /* properties */
    if (pos >= (uint16_t)(1 + rl_bytes + remaining)) return MQTT5_OK;

    uint32_t props_len;
    uint8_t props_vbi_bytes;
    err = mqtt5_decode_vbi(buf + pos, (uint16_t)(len - pos),
                           &props_len, &props_vbi_bytes);
    if (err != MQTT5_OK) return err;
    pos += props_vbi_bytes;

    if (props_len > (uint32_t)(len - pos)) return MQTT5_ERR_SIZE;
    /* Use uint32_t to avoid truncation when pos + props_len > 65535 */
    uint32_t props_end = (uint32_t)pos + props_len;

    while ((uint32_t)pos < props_end) {
        uint8_t prop_id = buf[pos++];
        switch (prop_id) {
        case MQTT5_PROP_SESSION_EXPIRY:
            if ((uint32_t)pos + 4 > props_end) return MQTT5_ERR_PROTOCOL;
            ack->session_expiry = get_u32(buf + pos);
            pos += 4;
            break;
        case MQTT5_PROP_RECEIVE_MAX:
            if ((uint32_t)pos + 2 > props_end) return MQTT5_ERR_PROTOCOL;
            ack->receive_max = get_u16(buf + pos);
            pos += 2;
            break;
        case MQTT5_PROP_MAX_QOS:
            if ((uint32_t)pos + 1 > props_end) return MQTT5_ERR_PROTOCOL;
            ack->max_qos = buf[pos++];
            break;
        default:
            err = skip_prop(connack_prop_kind(prop_id), buf, &pos, props_end);
            if (err != MQTT5_OK) return err;
            break;
        }
    }

    return MQTT5_OK;
}

mqtt5_err_t mqtt5_parse_publish(const uint8_t *buf, uint16_t len,
                                mqtt5_publish_t *pub)
{
    if (!buf || !pub) return MQTT5_ERR_PARAM;
    if (len < 5) return MQTT5_ERR_SIZE;
    if ((buf[0] >> 4) != 3) return MQTT5_ERR_PROTOCOL; /* not PUBLISH */

    pub->qos = (uint8_t)((buf[0] >> 1) & 0x03);
    if (pub->qos > 2) return MQTT5_ERR_PROTOCOL;  /* MQTT-3.3.1-4 */
    pub->retain = (buf[0] & 0x01) != 0;

    /* decode remaining length */
    uint32_t remaining;
    uint8_t rl_bytes;
    mqtt5_err_t err = mqtt5_decode_vbi(buf + 1, (uint16_t)(len - 1),
                                       &remaining, &rl_bytes);
    if (err != MQTT5_OK) return err;

    uint16_t pos = (uint16_t)(1 + rl_bytes);
    if (remaining > (uint32_t)(len - pos)) return MQTT5_ERR_SIZE;
    uint16_t packet_end = (uint16_t)(pos + remaining);

    /* topic */
    if (pos + 2 > packet_end) return MQTT5_ERR_PROTOCOL;
    uint16_t topic_len = get_u16(buf + pos);
    pos += 2;
    if (pos + topic_len > packet_end) return MQTT5_ERR_PROTOCOL;
    pub->topic = (const char *)(buf + pos);
    pub->topic_len = topic_len;
    pos += topic_len;

    /* packet id (only for QoS > 0) */
    if (pub->qos > 0) {
        if (pos + 2 > packet_end) return MQTT5_ERR_PROTOCOL;
        pub->packet_id = get_u16(buf + pos);
        pos += 2;
    } else {
        pub->packet_id = 0;
    }

    /* properties */
    pub->message_expiry = 0;
    if (pos < packet_end) {
        uint32_t props_len;
        uint8_t props_vbi_bytes;
        err = mqtt5_decode_vbi(buf + pos, (uint16_t)(packet_end - pos),
                               &props_len, &props_vbi_bytes);
        if (err != MQTT5_OK) return err;
        pos += props_vbi_bytes;

        if (props_len > (uint32_t)(packet_end - pos)) return MQTT5_ERR_PROTOCOL;
        /* Use uint32_t to avoid truncation when pos + props_len > 65535 */
        uint32_t props_end = (uint32_t)pos + props_len;

        while ((uint32_t)pos < props_end) {
            uint8_t prop_id = buf[pos++];
            switch (prop_id) {
            case MQTT5_PROP_MESSAGE_EXPIRY:
                if ((uint32_t)pos + 4 > props_end) return MQTT5_ERR_PROTOCOL;
                pub->message_expiry = get_u32(buf + pos);
                pos += 4;
                break;
            default:
                err = skip_prop(publish_prop_kind(prop_id), buf, &pos, props_end);
                if (err != MQTT5_OK) return err;
                break;
            }
        }
    }

    /* payload: everything after properties until packet end */
    if (pos < packet_end) {
        pub->payload = buf + pos;
        pub->payload_len = (uint16_t)(packet_end - pos);
    } else {
        pub->payload = NULL;
        pub->payload_len = 0;
    }

    return MQTT5_OK;
}
