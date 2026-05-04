// SPDX-License-Identifier: MIT
/*
 * Remote Telemetry — integration recipe
 *
 * Demonstrates cross-layer wheel integration:
 *   L0: ring_buffer, crc16, json_parser
 *   L2: mqtt_packet, at_command
 *
 * Flow: build JSON payload -> build MQTT PUBLISH packet ->
 *       buffer in ring_buffer for retry -> send via AT command
 *       (mock UART) -> verify CRC16 on AT response.
 *
 * Runs entirely on host. No OS, no heap, no hardware.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* --- wheels --------------------------------------------------------- */
#include "mqtt_packet.h"
#include "ring_buffer.h"
#include "at_command.h"
#include "crc16.h"
#include "json_parser.h"

/* ==================================================================== */
/* Mock UART HAL                                                        */
/* ==================================================================== */

#define MOCK_TX_BUF_SIZE  1024

typedef struct {
    uint8_t  buf[MOCK_TX_BUF_SIZE];
    uint16_t len;
} mock_uart_t;

static mock_uart_t g_mock_uart;

static void mock_uart_reset(void)
{
    memset(&g_mock_uart, 0, sizeof(g_mock_uart));
}

static void mock_uart_send(void *ctx, const uint8_t *data, uint16_t len)
{
    mock_uart_t *u = (mock_uart_t *)ctx;
    for (uint16_t i = 0; i < len && u->len < MOCK_TX_BUF_SIZE; i++) {
        u->buf[u->len++] = data[i];
    }
}

/* ==================================================================== */
/* Hex dump utility                                                     */
/* ==================================================================== */

static void hex_dump(const char *label, const uint8_t *data, size_t len)
{
    printf("--- %s (%zu bytes) ---\n", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    if (len % 16 != 0) {
        printf("\n");
    }
}

/* ==================================================================== */
/* Simple JSON builder (snprintf — json_parser is SAX/read-only)        */
/* ==================================================================== */

static int build_sensor_json(char *buf, size_t buf_size,
                             float temperature, float humidity,
                             float pressure, uint32_t seq)
{
    int n = snprintf(buf, buf_size,
        "{\"seq\":%lu,\"temp\":%.1f,\"hum\":%.1f,\"pres\":%.1f}",
        (unsigned long)seq, (double)temperature,
        (double)humidity, (double)pressure);
    if (n < 0 || (size_t)n >= buf_size) {
        return -1;
    }
    return n;
}

/* ==================================================================== */
/* JSON parser verification (parse what we just built)                  */
/* ==================================================================== */

typedef struct {
    char   last_key[32];
    int    field_count;
    float  temp;
    float  hum;
    float  pres;
    uint32_t seq;
} json_verify_ctx_t;

static float simple_atof(const char *s, uint16_t len)
{
    /* Minimal float parse for demo -- handles "25.3" style values */
    char tmp[32];
    if (len >= sizeof(tmp)) len = (uint16_t)(sizeof(tmp) - 1);
    memcpy(tmp, s, len);
    tmp[len] = '\0';
    float result = 0.0f;
    float sign = 1.0f;
    const char *p = tmp;
    if (*p == '-') { sign = -1.0f; p++; }
    while (*p >= '0' && *p <= '9') {
        result = result * 10.0f + (float)(*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        float frac = 0.1f;
        while (*p >= '0' && *p <= '9') {
            result += (float)(*p - '0') * frac;
            frac *= 0.1f;
            p++;
        }
    }
    return sign * result;
}

static uint32_t simple_atou(const char *s, uint16_t len)
{
    uint32_t result = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') break;
        result = result * 10 + (uint32_t)(s[i] - '0');
    }
    return result;
}

static void json_verify_cb(void *ctx, json_event_t event,
                            const char *value, uint16_t value_len)
{
    json_verify_ctx_t *v = (json_verify_ctx_t *)ctx;

    if (event == JSON_EVENT_KEY && value && value_len < sizeof(v->last_key)) {
        memcpy(v->last_key, value, value_len);
        v->last_key[value_len] = '\0';
    } else if (event == JSON_EVENT_NUMBER && value) {
        v->field_count++;
        if (strcmp(v->last_key, "seq") == 0) {
            v->seq = simple_atou(value, value_len);
        } else if (strcmp(v->last_key, "temp") == 0) {
            v->temp = simple_atof(value, value_len);
        } else if (strcmp(v->last_key, "hum") == 0) {
            v->hum = simple_atof(value, value_len);
        } else if (strcmp(v->last_key, "pres") == 0) {
            v->pres = simple_atof(value, value_len);
        }
    }
}

/* ==================================================================== */
/* Packet retry buffer (ring_buffer stores serialized MQTT packets)     */
/* ==================================================================== */

#define RETRY_BUF_SIZE  2048

typedef struct {
    ring_buffer_t rb;
    uint8_t       storage[RETRY_BUF_SIZE];
} retry_queue_t;

static void retry_queue_init(retry_queue_t *q)
{
    ring_buffer_init(&q->rb, q->storage, RETRY_BUF_SIZE);
}

/*
 * Enqueue a packet with a 2-byte length header so we can dequeue
 * whole packets later.  Format: [len_hi][len_lo][packet bytes...]
 */
static int retry_queue_push(retry_queue_t *q,
                            const uint8_t *pkt, uint16_t pkt_len)
{
    if (ring_buffer_free(&q->rb) < (uint16_t)(pkt_len + 2)) {
        return -1; /* not enough space */
    }
    uint8_t hdr[2] = {
        (uint8_t)(pkt_len >> 8),
        (uint8_t)(pkt_len & 0xFF)
    };
    ring_buffer_write(&q->rb, hdr, 2);
    ring_buffer_write(&q->rb, pkt, pkt_len);
    return 0;
}

/*
 * Peek the next packet without removing.  Returns packet length
 * or 0 if queue is empty.
 */
static uint16_t retry_queue_peek(retry_queue_t *q,
                                 uint8_t *out, uint16_t out_size)
{
    if (ring_buffer_count(&q->rb) < 2) {
        return 0;
    }
    uint8_t hdr[2];
    ring_buffer_peek(&q->rb, hdr, 2);
    uint16_t pkt_len = (uint16_t)((uint16_t)hdr[0] << 8 | hdr[1]);
    if (pkt_len > out_size || ring_buffer_count(&q->rb) < (uint16_t)(pkt_len + 2)) {
        return 0;
    }
    /* skip header, read packet */
    uint8_t tmp[2];
    ring_buffer_read(&q->rb, tmp, 2);
    uint16_t read = ring_buffer_read(&q->rb, out, pkt_len);
    /* put it back for potential retry (re-enqueue) */
    ring_buffer_write(&q->rb, hdr, 2);
    ring_buffer_write(&q->rb, out, read);
    return read;
}

/* Remove the front packet (after successful send). */
static void retry_queue_pop(retry_queue_t *q)
{
    if (ring_buffer_count(&q->rb) < 2) {
        return;
    }
    uint8_t hdr[2];
    ring_buffer_read(&q->rb, hdr, 2);
    uint16_t pkt_len = (uint16_t)((uint16_t)hdr[0] << 8 | hdr[1]);
    ring_buffer_skip(&q->rb, pkt_len);
}

/* ==================================================================== */
/* AT modem interaction (simulated)                                     */
/* ==================================================================== */

/*
 * Simulate feeding an AT response back through the parser byte by byte.
 * In real firmware this happens in a UART RX ISR / DMA callback.
 */
static at_command_evt_type_t simulate_at_response(at_command_t *at,
                                                   const char *response)
{
    at_command_evt_t evt;
    at_command_evt_type_t last = AT_COMMAND_EVT_NONE;
    size_t len = strlen(response);

    for (size_t i = 0; i < len; i++) {
        at_command_evt_type_t r = at_command_feed(at, (uint8_t)response[i], &evt);
        if (r != AT_COMMAND_EVT_NONE) {
            last = r;
        }
    }
    return last;
}

/* ==================================================================== */
/* main                                                                 */
/* ==================================================================== */

int main(void)
{
    int rc;

    printf("========================================\n");
    printf("  Remote Telemetry Integration Recipe\n");
    printf("========================================\n\n");

    /* ---- Step 1: Build JSON sensor payload ---- */

    printf("[Step 1] Build JSON sensor payload\n");

    char json_buf[128];
    rc = build_sensor_json(json_buf, sizeof(json_buf),
                           25.3f, 62.1f, 1013.2f, 42);
    if (rc < 0) {
        printf("  ERROR: JSON build failed\n");
        return 1;
    }
    printf("  JSON (%d bytes): %s\n\n", rc, json_buf);

    /* ---- Step 2: Verify JSON with json_parser ---- */

    printf("[Step 2] Verify JSON payload with json_parser\n");

    json_verify_ctx_t jctx;
    memset(&jctx, 0, sizeof(jctx));
    json_parser_t jp;
    json_parser_init(&jp, json_verify_cb, &jctx, 4);
    json_parser_err_t jerr = json_parser_feed(&jp, json_buf, (uint16_t)strlen(json_buf));
    if (jerr != JSON_PARSER_OK) {
        printf("  ERROR: JSON parse failed (%d)\n", jerr);
        return 1;
    }
    jerr = json_parser_finish(&jp);
    if (jerr != JSON_PARSER_OK) {
        printf("  ERROR: JSON finish failed (%d)\n", jerr);
        return 1;
    }
    printf("  Parsed %d fields: seq=%lu temp=%.1f hum=%.1f pres=%.1f\n\n",
           jctx.field_count, (unsigned long)jctx.seq,
           (double)jctx.temp, (double)jctx.hum, (double)jctx.pres);

    /* ---- Step 3: Build MQTT CONNECT packet ---- */

    printf("[Step 3] Build MQTT CONNECT packet\n");

    uint8_t mqtt_buf[512];
    mqtt_connect_opts_t conn_opts = {
        .client_id     = "telemetry-001",
        .username      = "device",
        .password      = "secret",
        .keepalive_s   = 60,
        .clean_session = true,
    };
    rc = mqtt_packet_build_connect(mqtt_buf, sizeof(mqtt_buf), &conn_opts);
    if (rc < 0) {
        printf("  ERROR: MQTT CONNECT build failed (%d)\n", rc);
        return 1;
    }
    printf("  CONNECT packet: %d bytes\n", rc);
    hex_dump("MQTT CONNECT", mqtt_buf, (size_t)rc);
    printf("\n");

    /* ---- Step 4: Build MQTT PUBLISH with JSON payload ---- */

    printf("[Step 4] Build MQTT PUBLISH packet\n");

    mqtt_publish_t pub = {
        .topic       = "device/telemetry-001/data",
        .payload     = (const uint8_t *)json_buf,
        .payload_len = strlen(json_buf),
        .packet_id   = 1,
        .qos         = 1,
        .retain      = false,
        .dup         = false,
    };
    uint8_t pub_buf[512];
    rc = mqtt_packet_build_publish(pub_buf, sizeof(pub_buf), &pub);
    if (rc < 0) {
        printf("  ERROR: MQTT PUBLISH build failed (%d)\n", rc);
        return 1;
    }
    int pub_len = rc;
    printf("  PUBLISH packet: %d bytes (topic=%s, QoS=%d)\n",
           pub_len, pub.topic, pub.qos);
    hex_dump("MQTT PUBLISH", pub_buf, (size_t)pub_len);
    printf("\n");

    /* ---- Step 5: Buffer packet in retry queue (ring_buffer) ---- */

    printf("[Step 5] Buffer PUBLISH in retry queue (ring_buffer)\n");

    retry_queue_t retry;
    retry_queue_init(&retry);

    rc = retry_queue_push(&retry, pub_buf, (uint16_t)pub_len);
    if (rc < 0) {
        printf("  ERROR: retry queue push failed\n");
        return 1;
    }
    printf("  Queued: %u bytes used / %u capacity\n",
           ring_buffer_count(&retry.rb),
           (unsigned)(RETRY_BUF_SIZE));

    /* Also queue a second packet to show multiple buffering */
    mqtt_publish_t pub2 = {
        .topic       = "device/telemetry-001/status",
        .payload     = (const uint8_t *)"online",
        .payload_len = 6,
        .packet_id   = 2,
        .qos         = 0,
        .retain      = true,
        .dup         = false,
    };
    uint8_t pub2_buf[256];
    rc = mqtt_packet_build_publish(pub2_buf, sizeof(pub2_buf), &pub2);
    if (rc >= 0) {
        retry_queue_push(&retry, pub2_buf, (uint16_t)rc);
        printf("  Queued status packet: +%d bytes\n", rc);
    }
    printf("  Total queued: %u bytes\n\n",
           ring_buffer_count(&retry.rb));

    /* ---- Step 6: Send via AT command (mock UART) ---- */

    printf("[Step 6] Send packets via AT command (mock UART)\n");

    mock_uart_reset();
    at_command_hal_t hal = {
        .send = mock_uart_send,
        .ctx  = &g_mock_uart,
    };
    at_command_t at;
    at_command_init(&at, &hal);

    /* 6a: Establish TCP connection to MQTT broker via AT */
    printf("  [6a] AT+CIPSTART (TCP connect to broker)\n");
    at_command_expect(&at, "CONNECT OK");
    at_command_send(&at, "AT+CIPSTART=\"TCP\",\"broker.example.com\",1883");
    printf("       Sent %u bytes to mock UART\n", g_mock_uart.len);

    /* Simulate modem response */
    at_command_evt_type_t evt;
    evt = simulate_at_response(&at, "CONNECT OK\r\n");
    printf("       Response: %s (evt=%d)\n",
           at_command_get_matched(&at),
           (int)evt);

    /* 6b: Send the MQTT PUBLISH (dequeue from retry buffer) */
    printf("  [6b] AT+CIPSEND (send MQTT PUBLISH)\n");
    uint8_t send_buf[512];
    uint16_t pkt_len = retry_queue_peek(&retry, send_buf, sizeof(send_buf));
    if (pkt_len == 0) {
        printf("  ERROR: retry queue empty\n");
        return 1;
    }

    /* Format AT+CIPSEND=<len> */
    char at_cmd[64];
    snprintf(at_cmd, sizeof(at_cmd), "AT+CIPSEND=%u", (unsigned)pkt_len);

    mock_uart_reset();
    at_command_expect(&at, ">");
    at_command_send(&at, at_cmd);

    /* Simulate prompt */
    evt = simulate_at_response(&at, ">");
    printf("       Got data prompt (evt=%d)\n", (int)evt);

    /* Send raw MQTT bytes after prompt */
    mock_uart_reset();
    at_command_send_raw(&at, send_buf, pkt_len);
    printf("       Sent %u raw MQTT bytes\n", g_mock_uart.len);
    hex_dump("Bytes sent to modem", g_mock_uart.buf, g_mock_uart.len);

    /* Simulate SEND OK */
    at_command_expect(&at, "SEND OK");
    evt = simulate_at_response(&at, "SEND OK\r\n");
    printf("       SEND result: %s (evt=%d)\n",
           at_command_get_matched(&at), (int)evt);

    /* Pop from retry queue on success */
    if (evt == AT_COMMAND_EVT_OK) {
        retry_queue_pop(&retry);
        printf("       Packet removed from retry queue\n");
    }
    printf("       Remaining in queue: %u bytes\n\n",
           ring_buffer_count(&retry.rb));

    /* ---- Step 7: Verify CRC16 on AT response data ---- */

    printf("[Step 7] CRC16 verification on response payload\n");

    /*
     * Simulate a response from the server that includes a CRC16 trailer.
     * In real systems the server might append CRC to binary data frames,
     * or the modem firmware appends it on raw TCP data.
     */
    const char *response_data = "PUBACK:OK";
    size_t resp_len = strlen(response_data);

    /* Compute CRC that the "server" would have appended */
    uint16_t expected_crc = crc16_modbus((const uint8_t *)response_data,
                                         resp_len);
    printf("  Response data: \"%s\"\n", response_data);
    printf("  Expected CRC16: 0x%04X\n", expected_crc);

    /* Build a frame: [data][crc_lo][crc_hi] */
    uint8_t frame[64];
    memcpy(frame, response_data, resp_len);
    frame[resp_len]     = (uint8_t)(expected_crc & 0xFF);
    frame[resp_len + 1] = (uint8_t)(expected_crc >> 8);

    /* Verify: CRC over the whole frame (data + CRC bytes) should be 0 */
    uint16_t check_crc = crc16_modbus(frame, resp_len + 2);
    printf("  Verification CRC (full frame): 0x%04X\n", check_crc);

    /* Also verify via streaming API */
    uint16_t stream_crc = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < resp_len + 2; i++) {
        stream_crc = crc16_modbus_step(stream_crc, frame[i]);
    }
    printf("  Streaming CRC check:           0x%04X\n", stream_crc);

    if (check_crc == 0x0000 && stream_crc == 0x0000) {
        printf("  CRC16 PASS: response integrity verified\n\n");
    } else {
        printf("  CRC16 FAIL: response corrupted!\n\n");
        return 1;
    }

    /* ---- Step 8: Summary ---- */

    printf("========================================\n");
    printf("  Integration Summary\n");
    printf("========================================\n");
    printf("  Wheels used:\n");
    printf("    L0  ring_buffer   - packet retry queue\n");
    printf("    L0  crc16         - response integrity check\n");
    printf("    L0  json_parser   - payload verification\n");
    printf("    L2  mqtt_packet   - CONNECT + PUBLISH encoding\n");
    printf("    L2  at_command    - modem AT command flow\n");
    printf("  Flow:\n");
    printf("    JSON build -> MQTT PUBLISH -> ring_buffer queue\n");
    printf("    -> AT+CIPSEND -> mock UART -> CRC16 verify\n");
    printf("  Result: ALL STEPS PASSED\n\n");

    return 0;
}
