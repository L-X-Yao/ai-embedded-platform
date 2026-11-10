// SPDX-License-Identifier: MIT
// LibFuzzer harness for air780eg -- coverage-guided fuzzing
//
// Plays the modem: raw bytes, well-formed URC lines and command replies in the
// order the input picks. Lines that fit must come back through the callbacks
// unchanged, and nothing may leave the driver's line buffer.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_air780eg.c ../air780eg.c -o fuzz_air780eg
// Run:     ./fuzz_air780eg -max_total_time=60 corpus_air780eg/

#include "../air780eg.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    const air780eg_t *dev;
    uint32_t now;
    uint32_t sends;
    uint32_t events;
    uint32_t nmea;
    uint32_t csq;
    int      rssi;
    uint32_t mqtt;
    char     topic[AIR780EG_TOPIC_MAX];
    const uint8_t *payload;
    uint16_t payload_len;
} world_t;

static void world_send(void *ctx, const uint8_t *data, uint16_t len) {
    world_t *w = ctx;
    if (!data) __builtin_trap();
    (void)len;
    w->sends++;
}

static uint32_t world_tick(void *ctx) {
    return ((world_t *)ctx)->now;
}

static void on_mqtt(void *user, const char *topic, const uint8_t *payload, uint16_t len) {
    world_t *w = user;
    const uint8_t *low = (const uint8_t *)w->dev->line_buf;
    const uint8_t *high = low + sizeof(w->dev->line_buf);

    if (strlen(topic) >= sizeof(w->topic)) __builtin_trap();
    if (payload < low || payload + len > high) __builtin_trap();
    strcpy(w->topic, topic);
    w->payload = payload;
    w->payload_len = len;
    w->mqtt++;
}

static void on_event(void *user) {
    ((world_t *)user)->events++;
}

static void on_nmea(void *user, const char *line) {
    if (line[0] != '$') __builtin_trap();
    ((world_t *)user)->nmea++;
}

static void on_csq(void *user, int rssi) {
    world_t *w = user;
    w->rssi = rssi;
    w->csq++;
}

static void type_line(air780eg_t *dev, const char *line, size_t len) {
    for (size_t i = 0; i < len; i++) air780eg_feed(dev, (uint8_t)line[i]);
    air780eg_feed(dev, '\r');
    air780eg_feed(dev, '\n');
}

static void check_invariants(const air780eg_t *dev) {
    if (dev->line_pos >= sizeof(dev->line_buf)) __builtin_trap();
    if (!memchr(dev->matched_line, 0, sizeof(dev->matched_line))) __builtin_trap();
    if (!memchr(dev->prev_line, 0, sizeof(dev->prev_line))) __builtin_trap();
}

// Printable text without the quote that ends the topic.
static size_t take_text(fuzz_input_t *in, char *out, size_t n) {
    size_t len = fuzz_take_str(in, out, n);
    for (size_t i = 0; i < len; i++) {
        if (out[i] < 0x20 || out[i] > 0x7E || out[i] == '"') out[i] = 'x';
    }
    return len;
}

// With no command pending a +MSUB line must reach the callback unchanged.
static void op_msub(air780eg_t *dev, world_t *w, fuzz_input_t *in) {
    char topic[AIR780EG_TOPIC_MAX + 16];
    char payload[320];
    static char line[AIR780EG_LINE_MAX * 2];
    size_t topic_len = take_text(in, topic, fuzz_u8(in) % (sizeof(topic) - 1));
    size_t payload_len = take_text(in, payload, fuzz_u16(in) % (sizeof(payload) - 1));
    int n = snprintf(line, sizeof(line), "+MSUB: \"%s\",%zu byte,\"%s\"", topic, payload_len, payload);

    // Raw bytes may have left half a line behind.
    air780eg_feed(dev, '\n');
    uint32_t before = w->mqtt;
    bool pending = dev->cmd_status == AIR780EG_CMD_PENDING;

    type_line(dev, line, (size_t)n);
    if (pending) return;
    if (topic_len >= AIR780EG_TOPIC_MAX) {
        if (w->mqtt != before) __builtin_trap();
        return;
    }
    if (w->mqtt != before + 1 || strcmp(w->topic, topic) != 0 ||
        w->payload_len != payload_len || memcmp(w->payload, payload, payload_len) != 0) {
        __builtin_trap();
    }
}

static void op_csq(air780eg_t *dev, world_t *w, fuzz_input_t *in) {
    char line[32];
    uint8_t digits[16];
    size_t count = 1 + fuzz_u8(in) % 12;
    fuzz_take(in, digits, count);

    size_t len = (size_t)snprintf(line, sizeof(line), "+CSQ: ");
    for (size_t i = 0; i < count; i++) line[len++] = (char)('0' + digits[i] % 10);
    memcpy(line + len, ",99", 3);
    type_line(dev, line, len + 3);
    if (w->csq > 0 && (w->rssi < 0 || (w->rssi > 31 && w->rssi != 99))) __builtin_trap();
}

static void op_command(air780eg_t *dev, world_t *w, fuzz_input_t *in) {
    static const char *const expects[] = { NULL, "OK", "+CSQ:", "CONNECT", "+MSUB" };
    char cmd[64];
    take_text(in, cmd, 1 + fuzz_u8(in) % 60);
    const char *expect = expects[fuzz_u8(in) % 5];
    bool pending = dev->cmd_status == AIR780EG_CMD_PENDING;
    uint32_t sends = w->sends;

    air780eg_err_t err = air780eg_send_cmd(dev, cmd, expect, fuzz_u16(in));
    if (pending) {
        if (err != AIR780EG_BUSY || w->sends != sends) __builtin_trap();
    } else if (err != AIR780EG_OK || w->sends != sends + 2 ||
               dev->cmd_status != AIR780EG_CMD_PENDING) {
        __builtin_trap();
    }
}

static void op_reply(air780eg_t *dev, fuzz_input_t *in) {
    static const char *const replies[] = {
        "OK", "ERROR", "+CME ERROR: 3", "+CMS ERROR: 1", "RDY", "CLOSED", "+PDP:DEACT",
        "+PDP DEACT", "+CGEV: NW PDN DEACT 1", "$GNGGA,1", "$GPRMC,2", "$GNRMC", "$GNGSA",
        "$GPGGA", "text", "", "CONNECT FAIL", "2,SEND FAIL",
    };
    size_t pick = fuzz_u8(in) % (sizeof(replies) / sizeof(replies[0]));

    air780eg_feed(dev, '\n');
    bool pending = dev->cmd_status == AIR780EG_CMD_PENDING;

    type_line(dev, replies[pick], strlen(replies[pick]));
    bool ok_ends = dev->expect[0] == '\0' || strcmp(dev->expect, "OK") == 0;
    if (pending && pick == 0 && ok_ends && dev->cmd_status != AIR780EG_CMD_OK) __builtin_trap();
    if (pending && pick == 0 && !ok_ends && dev->cmd_status != AIR780EG_CMD_PENDING) __builtin_trap();
    bool fails = (pick >= 1 && pick <= 3) || pick >= 16;
    if (pending && fails && dev->cmd_status != AIR780EG_CMD_ERROR) __builtin_trap();
}

static void op_poll(air780eg_t *dev, world_t *w, fuzz_input_t *in) {
    bool pending = dev->cmd_status == AIR780EG_CMD_PENDING;
    w->now += fuzz_u16(in);
    bool expired = (w->now - dev->cmd_start_ms) >= dev->cmd_timeout_ms;

    air780eg_poll(dev);
    if (pending && expired && dev->cmd_status != AIR780EG_CMD_TIMEOUT) __builtin_trap();
    if (pending && !expired && dev->cmd_status != AIR780EG_CMD_PENDING) __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    air780eg_t *dev = malloc(sizeof(*dev));
    world_t *w = calloc(1, sizeof(*w));
    if (dev && w) {
        air780eg_io_t io = { world_send, world_tick, w };
        w->dev = dev;
        if (air780eg_init(dev, &io) != AIR780EG_OK) __builtin_trap();
        air780eg_on_mqtt_rx(dev, on_mqtt, w);
        air780eg_on_disconnect(dev, on_event, w);
        air780eg_on_pdp_deact(dev, on_event, w);
        air780eg_on_restart(dev, on_event, w);
        air780eg_on_nmea(dev, on_nmea, w);
        air780eg_on_csq(dev, on_csq, w);

        for (int step = 0; step < 128 && in.left > 0; step++) {
            uint8_t raw[96];
            switch (fuzz_u8(&in) % 6) {
            case 0: op_msub(dev, w, &in); break;
            case 1: op_csq(dev, w, &in); break;
            case 2: op_command(dev, w, &in); break;
            case 3: op_reply(dev, &in); break;
            case 4: op_poll(dev, w, &in); break;
            default: {
                size_t n = fuzz_take(&in, raw, sizeof(raw));
                for (size_t i = 0; i < n; i++) air780eg_feed(dev, raw[i]);
                break;
            }
            }
            check_invariants(dev);
        }
    }
    free(dev);
    free(w);
    return 0;
}
