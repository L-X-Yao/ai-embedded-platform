// SPDX-License-Identifier: MIT
/*
 * mqtt_packet — MQTT 3.1.1 packet serializer/deserializer
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "mqtt_packet.h"
#include "contract.h"
#include <string.h>
#include <limits.h>

static int put_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
    return 2;
}

static int put_str(uint8_t *buf, const char *s)
{
    size_t slen = strlen(s);
    if (slen > UINT16_MAX) return -1;   /* caller must guard before calling */
    uint16_t len = (uint16_t)slen;
    put_u16(buf, len);
    memcpy(buf + 2, s, len);
    return 2 + (int)len;
}

static uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

int mqtt_packet_encode_remaining_length(uint8_t *buf, uint32_t length)
{
    if (!buf) return -1;
    if (length > 268435455u) return -1;
    int i = 0;
    do
    {
        uint8_t byte = (uint8_t)(length % 128);
        length /= 128;
        if (length > 0) byte |= 0x80;
        buf[i++] = byte;
    } while (length > 0 && i < 4);
    return i;
}

uint32_t mqtt_packet_decode_remaining_length(const uint8_t *buf, size_t len,
                                             int *bytes_used)
{
    if (!buf) { if (bytes_used) *bytes_used = 0; return 0; }
    uint32_t value = 0;
    uint32_t multiplier = 1;
    int i = 0;

    do
    {
        if ((size_t)i >= len)
        {
            if (bytes_used) *bytes_used = 0;
            return 0;
        }
        value += (uint32_t)(buf[i] & 0x7F) * multiplier;
        multiplier *= 128;
    } while ((buf[i++] & 0x80) != 0 && i < 4);

    if (i == 4 && (buf[3] & 0x80) != 0) {
        if (bytes_used) *bytes_used = 0;
        return 0;
    }

    if (bytes_used) *bytes_used = i;
    return value;
}

int mqtt_packet_build_connect(uint8_t *buf, size_t buf_size,
                              const mqtt_connect_opts_t *opts)
{
    if (!buf || !opts || !opts->client_id) return MQTT_PACKET_ERR_PARAM;

    uint8_t var_header[10];
    var_header[0] = 0x00; var_header[1] = 0x04;
    var_header[2] = 'M';  var_header[3] = 'Q';
    var_header[4] = 'T';  var_header[5] = 'T';
    var_header[6] = 0x04;

    if (opts->password && !opts->username) return MQTT_PACKET_ERR_PARAM;

    uint8_t flags = 0;
    if (opts->clean_session) flags |= 0x02;
    if (opts->username)      flags |= 0x80;
    if (opts->password)      flags |= 0x40;
    var_header[7] = flags;
    put_u16(&var_header[8], opts->keepalive_s);

    /* Each string carries a 16-bit length. Refusing longer ones here also
     * keeps the remaining length within what the fixed header can encode. */
    size_t client_len = strlen(opts->client_id);
    size_t user_len = opts->username ? strlen(opts->username) : 0;
    size_t pass_len = opts->password ? strlen(opts->password) : 0;
    if (client_len > UINT16_MAX || user_len > UINT16_MAX || pass_len > UINT16_MAX)
        return MQTT_PACKET_ERR_SIZE;

    size_t plen = 2 + client_len;
    if (opts->username) plen += 2 + user_len;
    if (opts->password) plen += 2 + pass_len;

    uint32_t remaining = 10 + (uint32_t)plen;
    uint8_t rl_buf[4];
    int rl_len = mqtt_packet_encode_remaining_length(rl_buf, remaining);

    size_t total = 1 + (size_t)rl_len + 10 + plen;
    if (total > buf_size) return MQTT_PACKET_ERR_SIZE;

    int pos = 0;
    buf[pos++] = (uint8_t)(MQTT_CONNECT << 4);
    memcpy(buf + pos, rl_buf, (size_t)rl_len); pos += rl_len;
    memcpy(buf + pos, var_header, 10); pos += 10;
    int n;
    n = put_str(buf + pos, opts->client_id);
    if (n < 0) return MQTT_PACKET_ERR_SIZE;
    pos += n;
    if (opts->username) {
        n = put_str(buf + pos, opts->username);
        if (n < 0) return MQTT_PACKET_ERR_SIZE;
        pos += n;
    }
    if (opts->password) {
        n = put_str(buf + pos, opts->password);
        if (n < 0) return MQTT_PACKET_ERR_SIZE;
        pos += n;
    }

    ENSURE((size_t)pos <= buf_size);
    return pos;
}

int mqtt_packet_build_publish(uint8_t *buf, size_t buf_size,
                              const mqtt_publish_t *pub)
{
    if (!buf || !pub || !pub->topic) return MQTT_PACKET_ERR_PARAM;
    if (pub->qos > 2) return MQTT_PACKET_ERR_PARAM;

    size_t raw_topic_len = strlen(pub->topic);
    if (raw_topic_len > UINT16_MAX) return MQTT_PACKET_ERR_SIZE;
    uint16_t topic_len = (uint16_t)raw_topic_len;
    uint32_t extra = (pub->qos > 0) ? 2u : 0u;
    if (pub->payload_len > UINT32_MAX - 2u - topic_len - extra)
        return MQTT_PACKET_ERR_SIZE;
    uint32_t remaining = 2 + topic_len + (uint32_t)pub->payload_len + extra;

    uint8_t rl_buf[4];
    int rl_len = mqtt_packet_encode_remaining_length(rl_buf, remaining);
    if (rl_len < 0) return MQTT_PACKET_ERR_SIZE;
    size_t total = 1 + (size_t)rl_len + remaining;
    if (total > buf_size) return MQTT_PACKET_ERR_SIZE;

    uint8_t byte0 = (uint8_t)(MQTT_PUBLISH << 4);
    if (pub->dup)    byte0 |= 0x08;
    byte0 = (uint8_t)(byte0 | (uint8_t)((pub->qos & 0x03u) << 1));
    if (pub->retain) byte0 |= 0x01;

    int pos = 0;
    buf[pos++] = byte0;
    memcpy(buf + pos, rl_buf, (size_t)rl_len); pos += rl_len;
    pos += put_u16(buf + pos, topic_len);
    memcpy(buf + pos, pub->topic, topic_len); pos += topic_len;
    if (pub->qos > 0) { pos += put_u16(buf + pos, pub->packet_id); }
    if (pub->payload && pub->payload_len > 0)
    {
        if (pub->payload_len > (size_t)INT_MAX) return MQTT_PACKET_ERR_SIZE;
        memcpy(buf + pos, pub->payload, pub->payload_len);
        pos += (int)pub->payload_len;
    }

    return pos;
}

int mqtt_packet_build_subscribe(uint8_t *buf, size_t buf_size,
                                uint16_t packet_id,
                                const char *topic, uint8_t qos)
{
    if (!buf || !topic) return MQTT_PACKET_ERR_PARAM;
    if (qos > 2) return MQTT_PACKET_ERR_PARAM;

    size_t raw_topic_len = strlen(topic);
    if (raw_topic_len > UINT16_MAX) return MQTT_PACKET_ERR_SIZE;
    uint16_t topic_len = (uint16_t)raw_topic_len;
    uint32_t remaining = 2u + 2u + (uint32_t)topic_len + 1u;

    uint8_t rl_buf[4];
    int rl_len = mqtt_packet_encode_remaining_length(rl_buf, remaining);
    int total = 1 + rl_len + (int)remaining;
    if ((size_t)total > buf_size) return MQTT_PACKET_ERR_SIZE;

    int pos = 0;
    buf[pos++] = (uint8_t)((MQTT_SUBSCRIBE << 4) | 0x02);
    memcpy(buf + pos, rl_buf, (size_t)rl_len); pos += rl_len;
    pos += put_u16(buf + pos, packet_id);
    pos += put_u16(buf + pos, topic_len);
    memcpy(buf + pos, topic, topic_len); pos += topic_len;
    buf[pos++] = qos;

    return pos;
}

int mqtt_packet_build_pingreq(uint8_t *buf, size_t buf_size)
{
    if (!buf || buf_size < 2) return MQTT_PACKET_ERR_SIZE;
    buf[0] = (uint8_t)(MQTT_PINGREQ << 4);
    buf[1] = 0;
    return 2;
}

int mqtt_packet_build_disconnect(uint8_t *buf, size_t buf_size)
{
    if (!buf || buf_size < 2) return MQTT_PACKET_ERR_SIZE;
    buf[0] = (uint8_t)(MQTT_DISCONNECT << 4);
    buf[1] = 0;
    return 2;
}

mqtt_packet_type_t mqtt_packet_get_type(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return 0;
    return (mqtt_packet_type_t)(buf[0] >> 4);
}

mqtt_packet_err_t mqtt_packet_parse_connack(const uint8_t *buf, size_t len,
                                            mqtt_connack_t *out)
{
    if (!buf || !out || len < 4) return MQTT_PACKET_ERR_PARAM;
    if ((buf[0] >> 4) != MQTT_CONNACK) return MQTT_PACKET_ERR_MALFORMED;
    out->session_present = (buf[2] & 0x01) != 0;
    out->return_code = buf[3];
    return MQTT_PACKET_OK;
}

mqtt_packet_err_t mqtt_packet_parse_publish(const uint8_t *buf, size_t len,
                                            mqtt_publish_t *out)
{
    if (!buf || !out || len < 4) return MQTT_PACKET_ERR_PARAM;
    if ((buf[0] >> 4) != MQTT_PUBLISH) return MQTT_PACKET_ERR_MALFORMED;

    out->dup = (buf[0] & 0x08) != 0;
    out->qos = (buf[0] >> 1) & 0x03;
    out->retain = (buf[0] & 0x01) != 0;

    int rl_bytes = 0;
    uint32_t remaining = mqtt_packet_decode_remaining_length(buf + 1, len - 1, &rl_bytes);
    if (rl_bytes == 0) return MQTT_PACKET_ERR_MALFORMED;

    size_t pos = 1 + (size_t)rl_bytes;
    if (pos + 2 > len) return MQTT_PACKET_ERR_MALFORMED;

    uint16_t topic_len = get_u16(buf + pos);
    pos += 2;
    if (pos + topic_len > len) return MQTT_PACKET_ERR_MALFORMED;

    out->topic = (const char *)(buf + pos);
    out->topic_len = topic_len;
    pos += topic_len;

    if (out->qos > 0)
    {
        if (pos + 2 > len) return MQTT_PACKET_ERR_MALFORMED;
        out->packet_id = get_u16(buf + pos);
        pos += 2;
    }
    else
    {
        out->packet_id = 0;
    }

    size_t header_size = 1 + (size_t)rl_bytes;
    size_t payload_start = pos;
    size_t packet_end = header_size + remaining;
    if (packet_end > len) packet_end = len;

    if (packet_end < payload_start) return MQTT_PACKET_ERR_MALFORMED;
    out->payload = (payload_start < packet_end) ? (buf + payload_start) : NULL;
    out->payload_len = packet_end - payload_start;

    return MQTT_PACKET_OK;
}

mqtt_packet_err_t mqtt_packet_parse_puback(const uint8_t *buf, size_t len,
                                           uint16_t *packet_id)
{
    if (!buf || !packet_id || len < 4) return MQTT_PACKET_ERR_PARAM;
    if ((buf[0] >> 4) != MQTT_PUBACK) return MQTT_PACKET_ERR_MALFORMED;
    *packet_id = get_u16(buf + 2);
    return MQTT_PACKET_OK;
}
