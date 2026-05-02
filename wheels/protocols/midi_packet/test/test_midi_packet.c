#include "test_harness.h"
#include "midi_packet.h"
#include <string.h>

/* --- Builder tests --- */

TEST(test_build_note_on)
{
    uint8_t buf[4];
    int n = midi_build_note_on(buf, sizeof(buf), 0, 60, 100);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[0], 0x90);
    ASSERT_EQ(buf[1], 60);
    ASSERT_EQ(buf[2], 100);
}

TEST(test_build_note_on_channel)
{
    uint8_t buf[4];
    int n = midi_build_note_on(buf, sizeof(buf), 9, 36, 127);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[0], 0x99);  /* channel 10 (drums) */
}

TEST(test_build_note_off)
{
    uint8_t buf[4];
    int n = midi_build_note_off(buf, sizeof(buf), 2, 64, 0);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[0], 0x82);
    ASSERT_EQ(buf[1], 64);
    ASSERT_EQ(buf[2], 0);
}

TEST(test_build_cc)
{
    uint8_t buf[4];
    int n = midi_build_cc(buf, sizeof(buf), 0, 7, 100);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[0], 0xB0);
    ASSERT_EQ(buf[1], 7);    /* volume */
    ASSERT_EQ(buf[2], 100);
}

TEST(test_build_program)
{
    uint8_t buf[4];
    int n = midi_build_program(buf, sizeof(buf), 0, 42);
    ASSERT_EQ(n, 2);
    ASSERT_EQ_HEX(buf[0], 0xC0);
    ASSERT_EQ(buf[1], 42);
}

TEST(test_build_pitch_bend_center)
{
    uint8_t buf[4];
    int n = midi_build_pitch_bend(buf, sizeof(buf), 0, 0);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[0], 0xE0);
    /* center = 8192 = 0x2000 => LSB=0x00, MSB=0x40 */
    ASSERT_EQ_HEX(buf[1], 0x00);
    ASSERT_EQ_HEX(buf[2], 0x40);
}

TEST(test_build_pitch_bend_extremes)
{
    uint8_t buf[4];
    /* min: -8192 => raw=0 => LSB=0, MSB=0 */
    int n = midi_build_pitch_bend(buf, sizeof(buf), 0, -8192);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[1], 0x00);
    ASSERT_EQ_HEX(buf[2], 0x00);

    /* max: +8191 => raw=16383=0x3FFF => LSB=0x7F, MSB=0x7F */
    n = midi_build_pitch_bend(buf, sizeof(buf), 0, 8191);
    ASSERT_EQ(n, 3);
    ASSERT_EQ_HEX(buf[1], 0x7F);
    ASSERT_EQ_HEX(buf[2], 0x7F);
}

/* --- Builder error cases --- */

TEST(test_build_null)
{
    ASSERT_EQ(midi_build_note_on(NULL, 3, 0, 60, 100), MIDI_PACKET_ERR_PARAM);
    ASSERT_EQ(midi_build_note_off(NULL, 3, 0, 60, 0), MIDI_PACKET_ERR_PARAM);
    ASSERT_EQ(midi_build_cc(NULL, 3, 0, 7, 100), MIDI_PACKET_ERR_PARAM);
    ASSERT_EQ(midi_build_program(NULL, 2, 0, 0), MIDI_PACKET_ERR_PARAM);
    ASSERT_EQ(midi_build_pitch_bend(NULL, 3, 0, 0), MIDI_PACKET_ERR_PARAM);
}

TEST(test_build_buf_too_small)
{
    uint8_t buf[2];
    ASSERT_EQ(midi_build_note_on(buf, 2, 0, 60, 100), MIDI_PACKET_ERR_SIZE);
    ASSERT_EQ(midi_build_cc(buf, 2, 0, 7, 100), MIDI_PACKET_ERR_SIZE);
    ASSERT_EQ(midi_build_pitch_bend(buf, 2, 0, 0), MIDI_PACKET_ERR_SIZE);

    uint8_t tiny[1];
    ASSERT_EQ(midi_build_program(tiny, 1, 0, 0), MIDI_PACKET_ERR_SIZE);
}

TEST(test_build_range_errors)
{
    uint8_t buf[4];
    /* channel > 15 */
    ASSERT_EQ(midi_build_note_on(buf, 4, 16, 60, 100), MIDI_PACKET_ERR_RANGE);
    /* note > 127 */
    ASSERT_EQ(midi_build_note_on(buf, 4, 0, 128, 100), MIDI_PACKET_ERR_RANGE);
    /* velocity > 127 */
    ASSERT_EQ(midi_build_note_on(buf, 4, 0, 60, 128), MIDI_PACKET_ERR_RANGE);
    /* pitch bend out of range */
    ASSERT_EQ(midi_build_pitch_bend(buf, 4, 0, -8193), MIDI_PACKET_ERR_RANGE);
    ASSERT_EQ(midi_build_pitch_bend(buf, 4, 0, 8192), MIDI_PACKET_ERR_RANGE);
}

/* --- Parser tests --- */

TEST(test_parse_note_on)
{
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    ASSERT_FALSE(midi_parse(&p, 0x90, &msg));   /* status */
    ASSERT_FALSE(midi_parse(&p, 60,   &msg));   /* note */
    ASSERT_TRUE(midi_parse(&p, 100,  &msg));     /* velocity -> complete */
    ASSERT_EQ(msg.type, MIDI_MSG_NOTE_ON);
    ASSERT_EQ(msg.channel, 0);
    ASSERT_EQ(msg.d.note.note, 60);
    ASSERT_EQ(msg.d.note.velocity, 100);
}

TEST(test_parse_note_on_velocity_zero)
{
    /* Note On with velocity 0 should be interpreted as Note Off */
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    ASSERT_FALSE(midi_parse(&p, 0x93, &msg));
    ASSERT_FALSE(midi_parse(&p, 48,   &msg));
    ASSERT_TRUE(midi_parse(&p, 0,    &msg));
    ASSERT_EQ(msg.type, MIDI_MSG_NOTE_OFF);
    ASSERT_EQ(msg.channel, 3);
}

TEST(test_parse_running_status)
{
    /* Two Note On messages using running status (no repeated status byte) */
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    /* First note */
    midi_parse(&p, 0x90, &msg);
    midi_parse(&p, 60, &msg);
    ASSERT_TRUE(midi_parse(&p, 100, &msg));
    ASSERT_EQ(msg.d.note.note, 60);

    /* Second note — running status, no 0x90 prefix */
    ASSERT_FALSE(midi_parse(&p, 62, &msg));
    ASSERT_TRUE(midi_parse(&p, 80, &msg));
    ASSERT_EQ(msg.type, MIDI_MSG_NOTE_ON);
    ASSERT_EQ(msg.d.note.note, 62);
    ASSERT_EQ(msg.d.note.velocity, 80);
}

TEST(test_parse_program_change)
{
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    ASSERT_FALSE(midi_parse(&p, 0xC5, &msg));
    ASSERT_TRUE(midi_parse(&p, 42, &msg));
    ASSERT_EQ(msg.type, MIDI_MSG_PROGRAM_CHANGE);
    ASSERT_EQ(msg.channel, 5);
    ASSERT_EQ(msg.d.pc.program, 42);
}

TEST(test_parse_pitch_bend)
{
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    /* Center: LSB=0x00, MSB=0x40 => raw=8192 => value=0 */
    ASSERT_FALSE(midi_parse(&p, 0xE0, &msg));
    ASSERT_FALSE(midi_parse(&p, 0x00, &msg));
    ASSERT_TRUE(midi_parse(&p, 0x40, &msg));
    ASSERT_EQ(msg.type, MIDI_MSG_PITCH_BEND);
    ASSERT_EQ(msg.d.pitch_bend.value, 0);
}

TEST(test_parse_sysex)
{
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    ASSERT_FALSE(midi_parse(&p, 0xF0, &msg));    /* SysEx start */
    ASSERT_FALSE(midi_parse(&p, 0x7E, &msg));    /* manufacturer ID */
    ASSERT_FALSE(midi_parse(&p, 0x01, &msg));    /* data */
    ASSERT_FALSE(midi_parse(&p, 0x02, &msg));    /* data */
    ASSERT_TRUE(midi_parse(&p, 0xF7, &msg));     /* SysEx end */

    ASSERT_EQ(msg.type, MIDI_MSG_SYSEX);
    ASSERT_EQ(msg.d.sysex.len, 3);
    ASSERT_EQ(msg.d.sysex.data[0], 0x7E);
    ASSERT_EQ(msg.d.sysex.data[1], 0x01);
    ASSERT_EQ(msg.d.sysex.data[2], 0x02);
}

TEST(test_parse_realtime_ignored)
{
    /* Real-time bytes (0xF8-0xFF) should be transparent to parsing */
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    midi_parse(&p, 0x90, &msg);
    midi_parse(&p, 60, &msg);
    ASSERT_FALSE(midi_parse(&p, 0xF8, &msg));   /* timing clock — ignored */
    ASSERT_TRUE(midi_parse(&p, 100, &msg));      /* velocity completes msg */
    ASSERT_EQ(msg.type, MIDI_MSG_NOTE_ON);
    ASSERT_EQ(msg.d.note.note, 60);
}

TEST(test_parse_null_params)
{
    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);

    ASSERT_FALSE(midi_parse(NULL, 0x90, &msg));
    ASSERT_FALSE(midi_parse(&p, 0x90, NULL));
}

/* --- Roundtrip tests --- */

TEST(test_roundtrip_note_on)
{
    uint8_t buf[4];
    int n = midi_build_note_on(buf, sizeof(buf), 3, 72, 110);
    ASSERT_EQ(n, 3);

    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);
    for (int i = 0; i < n - 1; i++) {
        ASSERT_FALSE(midi_parse(&p, buf[(size_t)i], &msg));
    }
    ASSERT_TRUE(midi_parse(&p, buf[(size_t)(n - 1)], &msg));
    ASSERT_EQ(msg.type, MIDI_MSG_NOTE_ON);
    ASSERT_EQ(msg.channel, 3);
    ASSERT_EQ(msg.d.note.note, 72);
    ASSERT_EQ(msg.d.note.velocity, 110);
}

TEST(test_roundtrip_pitch_bend)
{
    uint8_t buf[4];
    int n = midi_build_pitch_bend(buf, sizeof(buf), 7, -4000);
    ASSERT_EQ(n, 3);

    midi_parser_t p;
    midi_msg_t msg;
    midi_parser_init(&p);
    midi_parse(&p, buf[0], &msg);
    midi_parse(&p, buf[1], &msg);
    ASSERT_TRUE(midi_parse(&p, buf[2], &msg));
    ASSERT_EQ(msg.type, MIDI_MSG_PITCH_BEND);
    ASSERT_EQ(msg.channel, 7);
    ASSERT_EQ(msg.d.pitch_bend.value, -4000);
}

TEST_MAIN("midi_packet")
    RUN(test_build_note_on);
    RUN(test_build_note_on_channel);
    RUN(test_build_note_off);
    RUN(test_build_cc);
    RUN(test_build_program);
    RUN(test_build_pitch_bend_center);
    RUN(test_build_pitch_bend_extremes);
    RUN(test_build_null);
    RUN(test_build_buf_too_small);
    RUN(test_build_range_errors);
    RUN(test_parse_note_on);
    RUN(test_parse_note_on_velocity_zero);
    RUN(test_parse_running_status);
    RUN(test_parse_program_change);
    RUN(test_parse_pitch_bend);
    RUN(test_parse_sysex);
    RUN(test_parse_realtime_ignored);
    RUN(test_parse_null_params);
    RUN(test_roundtrip_note_on);
    RUN(test_roundtrip_pitch_bend);
TEST_MAIN_END()
