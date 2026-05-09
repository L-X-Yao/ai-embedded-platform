/*
 * prop_midi_packet.c — property-based tests for midi_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_midi_packet.c ../midi_packet.c -o prop_midi_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../midi_packet.h"
#include <string.h>

/* Property: note_on build then parse roundtrip */
PROPERTY(note_on_roundtrip, 10000) {
    uint8_t channel = (uint8_t)ARBI_RANGE(seed, 0, 15);
    uint8_t note = (uint8_t)ARBI_RANGE(seed, 0, 127);
    uint8_t velocity = (uint8_t)ARBI_RANGE(seed, 1, 127); /* velocity 0 => note off */

    uint8_t buf[4];
    int len = midi_build_note_on(buf, sizeof(buf), channel, note, velocity);
    PROP_ASSERT_EQ(len, 3);

    midi_parser_t parser;
    midi_parser_init(&parser);
    midi_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    bool got = false;
    for (int i = 0; i < len; i++) {
        if (midi_parse(&parser, buf[i], &msg))
            got = true;
    }
    PROP_ASSERT(got);
    PROP_ASSERT_EQ(msg.type, MIDI_MSG_NOTE_ON);
    PROP_ASSERT_EQ(msg.channel, channel);
    PROP_ASSERT_EQ(msg.d.note.note, note);
    PROP_ASSERT_EQ(msg.d.note.velocity, velocity);
}

/* Property: build functions are deterministic */
PROPERTY(build_determinism, 5000) {
    uint8_t ch = (uint8_t)ARBI_RANGE(seed, 0, 15);
    uint8_t note = (uint8_t)ARBI_RANGE(seed, 0, 127);
    uint8_t vel = (uint8_t)ARBI_RANGE(seed, 0, 127);

    uint8_t buf1[4], buf2[4];
    int len1 = midi_build_note_on(buf1, sizeof(buf1), ch, note, vel);
    int len2 = midi_build_note_on(buf2, sizeof(buf2), ch, note, vel);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: pitch bend roundtrip */
PROPERTY(pitch_bend_roundtrip, 10000) {
    uint8_t channel = (uint8_t)ARBI_RANGE(seed, 0, 15);
    int16_t value = (int16_t)ARBI_RANGE(seed, -8192, 8191);

    uint8_t buf[4];
    int len = midi_build_pitch_bend(buf, sizeof(buf), channel, value);
    PROP_ASSERT_EQ(len, 3);

    midi_parser_t parser;
    midi_parser_init(&parser);
    midi_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    bool got = false;
    for (int i = 0; i < len; i++) {
        if (midi_parse(&parser, buf[i], &msg))
            got = true;
    }
    PROP_ASSERT(got);
    PROP_ASSERT_EQ(msg.type, MIDI_MSG_PITCH_BEND);
    PROP_ASSERT_EQ(msg.channel, channel);
    PROP_ASSERT_EQ(msg.d.pitch_bend.value, value);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    midi_parser_t parser;
    midi_parser_init(&parser);

    uint8_t num_bytes = (uint8_t)ARBI_RANGE(seed, 1, 200);
    midi_msg_t msg;
    for (uint8_t i = 0; i < num_bytes; i++) {
        memset(&msg, 0, sizeof(msg));
        midi_parse(&parser, ARBI_UINT8(seed), &msg);
    }
}

TEST_MAIN("midi_packet properties") {
    RUN(note_on_roundtrip);
    RUN(build_determinism);
    RUN(pitch_bend_roundtrip);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
