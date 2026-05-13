// SPDX-License-Identifier: MIT
/*
 * mqtt_telemetry_demo -- integration recipe: MQTT client state machine
 *
 * Demonstrates the mqtt_client wheel driving a complete connect/publish/
 * disconnect lifecycle against a *mock TCP transport* that replies with
 * MQTT broker semantics in-process.  Unlike examples/remote_telemetry/
 * (which simulates LTE + AT-command + cellular framing), this demo
 * focuses on the wheel's STATE MACHINE: CONNECT -> CONNACK ->
 * periodic PUBLISH -> DISCONNECT.
 *
 * Combines four wheels:
 *   L1  bmp280       -- mocked temperature + pressure sensor (registers in RAM)
 *   L2  mqtt_packet  -- builds CONNACK responses from the mock broker side
 *   L3  sensor_hub   -- aggregates the bmp280 channels (temperature, pressure)
 *   L3  mqtt_client  -- state machine under test; publishes 10 cycles
 *
 * Build:  cmake -S . -B build && cmake --build build && ./build/mqtt_telemetry_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "bmp280.h"
#include "sensor_hub.h"
#include "mqtt_packet.h"
#include "mqtt_client.h"

/* ======================================================================
 * Sim configuration
 * ====================================================================== */

#define PUB_CYCLES        10
#define PUB_INTERVAL_MS  1000
#define KEEPALIVE_S        60
#define TOPIC_TEMP    "ai-embedded/dev0/temp"
#define TOPIC_PRESS   "ai-embedded/dev0/press"

/* ======================================================================
 * Mock virtual time -- monotonic millisecond counter
 * ====================================================================== */

static uint32_t g_now_ms = 0;

static uint32_t mock_get_tick_ms(void *ctx)
{
    (void)ctx;
    return g_now_ms;
}

static void mock_advance_time(uint32_t ms)
{
    g_now_ms += ms;
}

/* ======================================================================
 * Mock TCP transport
 *
 * Captures every byte the client writes (so the demo can show packet
 * type & total bytes), and pretends to be a broker by injecting a
 * CONNACK after CONNECT.  All in-process; no real socket.
 * ====================================================================== */

static uint32_t mock_tcp_bytes_out;
static uint32_t mock_tcp_publish_count;
static uint32_t mock_tcp_pingreq_count;

/* The mqtt_client instance is parked here so the deferred broker pump
 * (mock_tcp_pump_replies) can call mqtt_client_feed without circular
 * argument plumbing. */
static mqtt_client_t *g_client_for_loopback;

/* Pending broker-side replies.  We can't feed them synchronously from
 * inside the send callback because mqtt_client_connect() only sets state
 * to CONNECTING after send_raw() returns -- delivering CONNACK mid-send
 * would be dropped by dispatch_connack().  So queue them and let the
 * caller pump after mqtt_client_connect/publish returns. */
#define MOCK_REPLY_BUF_SIZE  64
static uint8_t  mock_reply_buf[MOCK_REPLY_BUF_SIZE];
static uint16_t mock_reply_len;

static void mock_queue_reply(const uint8_t *data, uint16_t len)
{
    if (mock_reply_len + len > MOCK_REPLY_BUF_SIZE) {
        printf("    [mock] reply buffer overflow (drop)\n");
        return;
    }
    memcpy(&mock_reply_buf[mock_reply_len], data, len);
    mock_reply_len = (uint16_t)(mock_reply_len + len);
}

/* Called by the application loop AFTER mqtt_client_connect/publish/etc.
 * returns, to deliver any queued broker replies into the state machine. */
static void mock_tcp_pump_replies(void)
{
    if (mock_reply_len == 0 || !g_client_for_loopback) return;
    uint16_t len = mock_reply_len;
    /* Snapshot first so a synchronous mqtt_client_feed callback can't
     * race the buffer pointer in case it re-enters via on_message. */
    mock_reply_len = 0;
    mqtt_client_feed(g_client_for_loopback, mock_reply_buf, len);
}

static int mock_tcp_send(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    mock_tcp_bytes_out += len;

    mqtt_packet_type_t t = mqtt_packet_get_type(data, len);
    switch (t) {
    case MQTT_CONNECT: {
        printf("    [tcp tx] CONNECT  (%u bytes)\n", (unsigned)len);
        /* Queue a CONNACK (fixed header 0x20 0x02 + flags 0x00 + rc 0x00). */
        uint8_t connack[] = { 0x20, 0x02, 0x00, 0x00 };
        mock_queue_reply(connack, sizeof(connack));
        break;
    }
    case MQTT_PUBLISH:
        mock_tcp_publish_count++;
        printf("    [tcp tx] PUBLISH  (%u bytes)\n", (unsigned)len);
        break;
    case MQTT_PINGREQ: {
        mock_tcp_pingreq_count++;
        printf("    [tcp tx] PINGREQ  (%u bytes)\n", (unsigned)len);
        /* Queue a PINGRESP so the next pump call keeps the session alive. */
        uint8_t pingresp[] = { 0xD0, 0x00 };
        mock_queue_reply(pingresp, sizeof(pingresp));
        break;
    }
    case MQTT_DISCONNECT:
        printf("    [tcp tx] DISCONNECT (%u bytes)\n", (unsigned)len);
        break;
    default:
        printf("    [tcp tx] type=%d (%u bytes)\n", (int)t, (unsigned)len);
        break;
    }
    return 0;  /* 0 == success per mqtt_client_io_t.send contract */
}

/* ======================================================================
 * Mock BMP280 I2C -- vary the readings each cycle so PUBLISH payloads
 * carry distinct data.  Calibration values copied from env_monitor.
 * ====================================================================== */

static uint8_t  bmp_regs[256];
static uint32_t bmp_cycle;

static void bmp_set_adc_for_cycle(uint32_t cycle)
{
    /* Sweep adc_t around a nominal point so temperature varies cycle to
     * cycle.  Pressure stays near 1 atm with a small drift. */
    uint32_t adc_t = 519888 + (cycle % 5) * 800;
    uint32_t adc_p = 415148 + (cycle % 5) * 50;

    bmp_regs[0xF7] = (uint8_t)(adc_p >> 12);
    bmp_regs[0xF8] = (uint8_t)(adc_p >> 4);
    bmp_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    bmp_regs[0xFA] = (uint8_t)(adc_t >> 12);
    bmp_regs[0xFB] = (uint8_t)(adc_t >> 4);
    bmp_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
}

static void bmp_mock_reset(void)
{
    memset(bmp_regs, 0, sizeof(bmp_regs));
    bmp_cycle = 0;

    bmp_regs[0xD0] = 0x58;   /* chip id */

    /* Calibration block (BMP280 datasheet example values) */
    bmp_regs[0x88] = 0x70; bmp_regs[0x89] = 0x6B;
    bmp_regs[0x8A] = 0x43; bmp_regs[0x8B] = 0x67;
    bmp_regs[0x8C] = 0x18; bmp_regs[0x8D] = 0xFC;
    bmp_regs[0x8E] = 0x7D; bmp_regs[0x8F] = 0x8E;
    bmp_regs[0x90] = 0x43; bmp_regs[0x91] = 0xD6;
    bmp_regs[0x92] = 0xD0; bmp_regs[0x93] = 0x0B;
    bmp_regs[0x94] = 0x27; bmp_regs[0x95] = 0x0B;
    bmp_regs[0x96] = 0x8C; bmp_regs[0x97] = 0x00;
    bmp_regs[0x98] = 0xF9; bmp_regs[0x99] = 0xFF;
    bmp_regs[0x9A] = 0x8C; bmp_regs[0x9B] = 0x3C;
    bmp_regs[0x9C] = 0xF8; bmp_regs[0x9D] = 0xC6;
    bmp_regs[0x9E] = 0x70; bmp_regs[0x9F] = 0x17;

    bmp_regs[0xF3] = 0x00;   /* status: conversion ready */

    bmp_set_adc_for_cycle(0);
}

static bool bmp_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    bmp_regs[reg] = val;
    return true;
}

static bool bmp_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = bmp_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const bmp280_i2c_t bmp_i2c = {
    .write_reg = bmp_write_reg,
    .read_regs = bmp_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * sensor_hub adapters -- bridge bmp280_read() to sensor_hub_read_fn
 * ====================================================================== */

static sensor_hub_err_t adapter_temp(void *ctx, float *value)
{
    bmp280_t *dev = (bmp280_t *)ctx;
    bmp280_data_t d;
    if (bmp280_read(dev, &d) != BMP280_OK) return SENSOR_HUB_ERR_READ;
    *value = d.temperature_c;
    return SENSOR_HUB_OK;
}

static sensor_hub_err_t adapter_press(void *ctx, float *value)
{
    bmp280_t *dev = (bmp280_t *)ctx;
    bmp280_data_t d;
    if (bmp280_read(dev, &d) != BMP280_OK) return SENSOR_HUB_ERR_READ;
    *value = d.pressure_pa;
    return SENSOR_HUB_OK;
}

/* ======================================================================
 * mqtt_client event hooks -- track state transitions for verification
 * ====================================================================== */

static int g_connected_seen = 0;
static int g_disconnected_seen = 0;
static int g_state_transitions = 0;

static const char *state_name(mqtt_client_state_t s)
{
    switch (s) {
    case MQTT_CLIENT_DISCONNECTED: return "DISCONNECTED";
    case MQTT_CLIENT_CONNECTING:   return "CONNECTING";
    case MQTT_CLIENT_CONNECTED:    return "CONNECTED";
    default:                       return "?";
    }
}

static void on_event(void *ctx, mqtt_client_state_t new_state, uint8_t reason)
{
    (void)ctx;
    g_state_transitions++;
    if (new_state == MQTT_CLIENT_CONNECTED)    g_connected_seen = 1;
    if (new_state == MQTT_CLIENT_DISCONNECTED) g_disconnected_seen = 1;
    printf("    [mqtt evt] -> %s (reason=0x%02X)\n",
           state_name(new_state), (unsigned)reason);
}

/* ======================================================================
 * Globals
 * ====================================================================== */

static bmp280_t     g_bmp;
static sensor_hub_t g_hub;
static mqtt_client_t g_client;

/* ======================================================================
 * main scenario
 * ====================================================================== */

int main(void)
{
    printf("=== mqtt_telemetry_demo -- MQTT client state machine ===\n");
    printf("Wheels: bmp280 + sensor_hub + mqtt_packet + mqtt_client\n");
    printf("==========================================================\n");

    /* ---- 1. Boot the mocked BMP280 driver. */
    bmp_mock_reset();
    bmp280_cfg_t bmp_cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_1,
        .osrs_p = BMP280_OVERSAMPLING_1,
    };
    if (bmp280_init(&g_bmp, &bmp_i2c, &bmp_cfg) != BMP280_OK) {
        printf("[fail] bmp280_init\n");
        return 1;
    }
    printf("[init] bmp280 ready (mocked I2C)\n");

    /* ---- 2. Register channels on sensor_hub. */
    if (sensor_hub_init(&g_hub) != SENSOR_HUB_OK) {
        printf("[fail] sensor_hub_init\n");
        return 1;
    }
    sensor_hub_register(&g_hub, "temp",  "C",  adapter_temp,  &g_bmp);
    sensor_hub_register(&g_hub, "press", "Pa", adapter_press, &g_bmp);
    printf("[init] sensor_hub channels=%u\n", (unsigned)sensor_hub_count(&g_hub));

    /* ---- 3. Bring up mqtt_client with the mock loopback transport. */
    const mqtt_client_io_t io = {
        .send         = mock_tcp_send,
        .get_tick_ms  = mock_get_tick_ms,
        .ctx          = NULL,
    };
    const mqtt_client_cfg_t cfg = {
        .client_id          = "telemetry-demo-01",
        .username           = NULL,
        .password           = NULL,
        .keepalive_s        = KEEPALIVE_S,
        .clean_session      = true,
        .connect_timeout_ms = 5000,
        .ack_timeout_ms     = 2000,
    };
    if (mqtt_client_init(&g_client, &io, &cfg) != MQTT_CLIENT_OK) {
        printf("[fail] mqtt_client_init\n");
        return 1;
    }
    mqtt_client_on_event(&g_client, on_event, NULL);
    g_client_for_loopback = &g_client;
    printf("[init] mqtt_client initial state = %s\n",
           state_name(mqtt_client_get_state(&g_client)));

    /* ---- 4. Connect.  mqtt_client_connect sends CONNECT (which our
     *        mock queues a CONNACK reply for) and transitions to
     *        CONNECTING.  We then pump the queued CONNACK into
     *        mqtt_client_feed, which lifts the state machine to
     *        CONNECTED. */
    printf("\n--- Connect handshake ---\n");
    if (mqtt_client_connect(&g_client) != MQTT_CLIENT_OK) {
        printf("[fail] mqtt_client_connect\n");
        return 1;
    }
    /* Deliver queued broker reply (CONNACK). */
    mock_tcp_pump_replies();

    /* Drive the poll once so any timing logic runs. */
    mqtt_client_poll(&g_client);

    if (mqtt_client_get_state(&g_client) != MQTT_CLIENT_CONNECTED) {
        printf("[fail] client did not reach CONNECTED state\n");
        return 1;
    }

    /* ---- 5. Periodic publish loop. */
    printf("\n--- Publishing %d cycles at %d ms intervals ---\n",
           PUB_CYCLES, PUB_INTERVAL_MS);

    char payload[64];

    for (uint32_t cycle = 0; cycle < PUB_CYCLES; cycle++) {
        /* Advance virtual time and update sensor mock. */
        mock_advance_time(PUB_INTERVAL_MS);
        bmp_cycle++;
        bmp_set_adc_for_cycle(bmp_cycle);

        /* Read all sensor channels through sensor_hub. */
        if (sensor_hub_poll(&g_hub) != SENSOR_HUB_OK) {
            printf("[fail] sensor_hub_poll on cycle %u\n", (unsigned)cycle);
            return 1;
        }

        float temp_c = 0.0f, press_pa = 0.0f;
        sensor_hub_get(&g_hub, 0, &temp_c,   NULL);
        sensor_hub_get(&g_hub, 1, &press_pa, NULL);

        /* Build a tiny ASCII payload and publish to each topic
         * (one QoS 0 publish per channel = 2 per cycle). */
        int n = snprintf(payload, sizeof(payload),
                         "{\"cycle\":%u,\"t\":%.2f}", (unsigned)cycle,
                         (double)temp_c);
        if (n < 0) { n = 0; }
        mqtt_client_err_t rc = mqtt_client_publish(&g_client, TOPIC_TEMP,
                                                   (const uint8_t *)payload,
                                                   (uint16_t)n, 0);
        if (rc != MQTT_CLIENT_OK) {
            printf("[fail] publish temp cycle %u rc=%d\n",
                   (unsigned)cycle, rc);
            return 1;
        }

        n = snprintf(payload, sizeof(payload),
                     "{\"cycle\":%u,\"p\":%.0f}", (unsigned)cycle,
                     (double)press_pa);
        if (n < 0) { n = 0; }
        rc = mqtt_client_publish(&g_client, TOPIC_PRESS,
                                 (const uint8_t *)payload,
                                 (uint16_t)n, 0);
        if (rc != MQTT_CLIENT_OK) {
            printf("[fail] publish press cycle %u rc=%d\n",
                   (unsigned)cycle, rc);
            return 1;
        }

        printf("  cycle %2u  t=%6.2fC  p=%9.0fPa\n",
               (unsigned)cycle, (double)temp_c, (double)press_pa);

        /* Pump the state machine.  In a real device this is the only
         * call that runs the keepalive / inflight-ack timers. */
        mqtt_client_poll(&g_client);

        /* Deliver any queued broker replies (e.g. PINGRESP for keepalive). */
        mock_tcp_pump_replies();
    }

    /* ---- 6. Disconnect cleanly. */
    printf("\n--- Clean disconnect ---\n");
    if (mqtt_client_disconnect(&g_client) != MQTT_CLIENT_OK) {
        printf("[fail] mqtt_client_disconnect\n");
        return 1;
    }
    if (mqtt_client_get_state(&g_client) != MQTT_CLIENT_DISCONNECTED) {
        printf("[fail] client did not reach DISCONNECTED state\n");
        return 1;
    }

    /* ---- 7. Stats + self-check. */
    printf("\n--- Stats ---\n");
    printf("  state transitions seen : %d\n", g_state_transitions);
    printf("  connected event seen   : %s\n", g_connected_seen ? "YES" : "no");
    printf("  disconnected event seen: %s\n", g_disconnected_seen ? "YES" : "no");
    printf("  bytes sent over TCP    : %lu\n", (unsigned long)mock_tcp_bytes_out);
    printf("  PUBLISH packets        : %lu (expected %u)\n",
           (unsigned long)mock_tcp_publish_count, PUB_CYCLES * 2u);
    printf("  PINGREQ packets        : %lu\n",
           (unsigned long)mock_tcp_pingreq_count);

    /* Verify what we promised */
    if (!g_connected_seen) {
        printf("[fail] never reached CONNECTED\n");
        return 1;
    }
    if (!g_disconnected_seen) {
        printf("[fail] never reached DISCONNECTED\n");
        return 1;
    }
    if (mock_tcp_publish_count != (uint32_t)(PUB_CYCLES * 2u)) {
        printf("[fail] publish count mismatch\n");
        return 1;
    }
    if (g_state_transitions < 2) {
        /* expect at least CONNECTING (via CONNECT send) and CONNECTED
         * (via CONNACK feed) and DISCONNECTED (via disconnect call). */
        printf("[fail] too few state transitions: %d\n", g_state_transitions);
        return 1;
    }

    printf("\n=== mqtt_telemetry_demo complete ===\n");
    return 0;
}
