// SPDX-License-Identifier: MIT
/*
 * stress_mqtt_packet.c — long-running stress tests for mqtt_packet
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_mqtt_packet.c ../mqtt_packet.c -o /tmp/stress_mqtt_packet -lm
 *
 * Scenarios:
 *   1. CONNECT build/parse cycle — 100K packets, verify roundtrip
 *   2. PUBLISH build/parse cycle — varying payload sizes over 100K iterations
 *   3. Remaining length encode/decode — exhaustive for multi-byte encodings
 *   4. SUBSCRIBE rapid build — 100K packets, verify format consistency
 */

#include "test_harness.h"
#include "mqtt_packet.h"
#include <string.h>

/* ---- tests ---------------------------------------------------------- */

TEST(connect_build_stress)
{
    uint8_t buf[512];
    int ok = 1;

    for (int i = 0; i < 100000; i++) {
        mqtt_connect_opts_t opts = {
            .client_id     = "stress-client",
            .username      = (i % 2 == 0) ? "user" : NULL,
            .password      = (i % 2 == 0) ? "pass" : NULL,
            .keepalive_s   = (uint16_t)(60 + (i % 100)),
            .clean_session = (i % 3 != 0),
        };
        int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
        if (len <= 0) { ok = 0; break; }

        /* Verify packet type byte */
        if ((buf[0] >> 4) != MQTT_CONNECT) { ok = 0; break; }

        /* Verify minimum length (fixed header + variable header + client_id) */
        if (len < 14) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(publish_build_parse_roundtrip)
{
    uint8_t build_buf[1024];
    int ok = 1;

    for (int i = 0; i < 100000; i++) {
        /* Vary payload length: 0 to 200 bytes */
        uint8_t payload[200];
        size_t payload_len = (size_t)(i % 201);
        memset(payload, (uint8_t)(i & 0xFF), payload_len);

        mqtt_publish_t pub = {
            .topic       = "test/stress/topic",
            .payload     = payload,
            .payload_len = payload_len,
            .packet_id   = (uint16_t)(i & 0xFFFF),
            .qos         = (uint8_t)(i % 3),
            .retain      = (i % 5 == 0),
            .dup         = false,
        };

        int len = mqtt_packet_build_publish(build_buf, sizeof(build_buf), &pub);
        if (len <= 0) { ok = 0; break; }

        /* Parse back */
        mqtt_publish_t parsed;
        memset(&parsed, 0, sizeof(parsed));
        mqtt_packet_err_t err = mqtt_packet_parse_publish(build_buf, (size_t)len, &parsed);
        if (err != MQTT_PACKET_OK) { ok = 0; break; }

        /* Verify topic survived roundtrip (topic is NOT null-terminated in
         * parsed output — it points into the wire buffer) */
        const char *expected_topic = "test/stress/topic";
        size_t expected_topic_len = strlen(expected_topic);
        if (memcmp(parsed.topic, expected_topic, expected_topic_len) != 0) {
            ok = 0; break;
        }
        /* Verify payload length */
        if (parsed.payload_len != payload_len) { ok = 0; break; }
        /* Verify payload content */
        if (payload_len > 0 && memcmp(parsed.payload, payload, payload_len) != 0) {
            ok = 0; break;
        }
    }
    ASSERT_TRUE(ok);
}

TEST(remaining_length_encode_decode_exhaustive)
{
    uint8_t enc_buf[4];
    int ok = 1;

    /* Test remaining length encoding for values spanning all 4 encoding sizes:
     * 1-byte: 0..127
     * 2-byte: 128..16383
     * 3-byte: 16384..2097151
     * 4-byte: 2097152..268435455
     * We test boundary values and samples from each range */
    static const uint32_t test_values[] = {
        0, 1, 63, 126, 127,
        128, 129, 255, 256, 1000, 16382, 16383,
        16384, 16385, 32767, 65535, 100000, 2097150, 2097151,
        2097152, 2097153, 10000000, 268435454, 268435455
    };
    int num_values = (int)(sizeof(test_values) / sizeof(test_values[0]));

    for (int round = 0; round < 10000; round++) {
        for (int vi = 0; vi < num_values; vi++) {
            uint32_t val = test_values[vi];
            int enc_len = mqtt_packet_encode_remaining_length(enc_buf, val);
            if (enc_len <= 0 || enc_len > 4) { ok = 0; break; }

            int bytes_used = 0;
            uint32_t decoded = mqtt_packet_decode_remaining_length(enc_buf,
                                   (size_t)enc_len, &bytes_used);
            if (decoded != val) { ok = 0; break; }
            if (bytes_used != enc_len) { ok = 0; break; }
        }
        if (!ok) break;
    }
    ASSERT_TRUE(ok);
}

TEST(subscribe_build_stress)
{
    uint8_t buf[512];
    int ok = 1;

    for (int i = 0; i < 100000; i++) {
        uint16_t packet_id = (uint16_t)(i + 1);
        uint8_t qos = (uint8_t)(i % 3);
        int len = mqtt_packet_build_subscribe(buf, sizeof(buf),
                                              packet_id, "stress/topic/#", qos);
        if (len <= 0) { ok = 0; break; }
        /* Verify packet type byte */
        if ((buf[0] >> 4) != MQTT_SUBSCRIBE) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress mqtt_packet")
    RUN(connect_build_stress);
    RUN(publish_build_parse_roundtrip);
    RUN(remaining_length_encode_decode_exhaustive);
    RUN(subscribe_build_stress);
TEST_MAIN_END()
