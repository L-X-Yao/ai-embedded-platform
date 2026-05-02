// SPDX-License-Identifier: MIT
/*
 * midi_packet — MIDI message encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Streaming byte-at-a-time parser with running status support.
 * Builder functions for Note On, Note Off, Control Change,
 * Program Change, Pitch Bend, and SysEx messages.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef MIDI_PACKET_H
#define MIDI_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Status byte constants --- */

#define MIDI_NOTE_OFF       0x80
#define MIDI_NOTE_ON        0x90
#define MIDI_CONTROL_CHANGE 0xB0
#define MIDI_PROGRAM_CHANGE 0xC0
#define MIDI_PITCH_BEND     0xE0
#define MIDI_SYSEX_START    0xF0
#define MIDI_SYSEX_END      0xF7

/* --- Error codes --- */

typedef enum {
    MIDI_PACKET_OK          =  0,
    MIDI_PACKET_ERR_PARAM   = -1,
    MIDI_PACKET_ERR_SIZE    = -2,
    MIDI_PACKET_ERR_RANGE   = -3,
} midi_packet_err_t;

/* --- Message types --- */

typedef enum {
    MIDI_MSG_NONE           = 0,
    MIDI_MSG_NOTE_OFF       = 1,
    MIDI_MSG_NOTE_ON        = 2,
    MIDI_MSG_CONTROL_CHANGE = 3,
    MIDI_MSG_PROGRAM_CHANGE = 4,
    MIDI_MSG_PITCH_BEND     = 5,
    MIDI_MSG_SYSEX          = 6,
} midi_msg_type_t;

/* --- Parsed message --- */

typedef struct {
    midi_msg_type_t type;
    uint8_t         channel;    /* 0-15 */
    union {
        struct { uint8_t note; uint8_t velocity; } note;       /* note on/off */
        struct { uint8_t number; uint8_t value; } cc;          /* control change */
        struct { uint8_t program; } pc;                         /* program change */
        struct { int16_t value; } pitch_bend;                   /* -8192..+8191 */
        struct { const uint8_t *data; size_t len; } sysex;     /* SysEx payload (between F0..F7) */
    } d;
} midi_msg_t;

/* --- Streaming parser context --- */

#ifndef MIDI_SYSEX_MAX
#define MIDI_SYSEX_MAX 128
#endif

typedef struct {
    uint8_t  running_status;       /* last status byte for running status */
    uint8_t  buf[2];               /* data byte accumulator */
    uint8_t  buf_pos;              /* bytes collected so far */
    uint8_t  expected;             /* data bytes expected for current status */
    bool     in_sysex;             /* inside SysEx message */
    bool     sysex_overflow;       /* true if SysEx exceeded MIDI_SYSEX_MAX */
    uint8_t  sysex_buf[MIDI_SYSEX_MAX];
    size_t   sysex_len;
} midi_parser_t;

/** Reset parser to initial state. @thread_safe: caller_must_lock */
void midi_parser_init(midi_parser_t *p);

/**
 * Feed one byte into the parser. Returns true when a complete message
 * is available in *out. Call repeatedly for each received byte.
 * Supports running status: a data byte without a preceding status byte
 * re-uses the most recent channel voice status.
 *
 * @thread_safe: caller_must_lock
 */
bool midi_parse(midi_parser_t *p, uint8_t byte, midi_msg_t *out);

/* --- Builder functions --- */
/* All return encoded length (positive) or negative error.           */
/* buf must be caller-provided; cap is buffer capacity in bytes.     */

/** Build Note On message (3 bytes). @thread_safe: yes (pure function) */
int midi_build_note_on(uint8_t *buf, size_t cap,
                       uint8_t channel, uint8_t note, uint8_t velocity);

/** Build Note Off message (3 bytes). @thread_safe: yes (pure function) */
int midi_build_note_off(uint8_t *buf, size_t cap,
                        uint8_t channel, uint8_t note, uint8_t velocity);

/** Build Control Change message (3 bytes). @thread_safe: yes (pure function) */
int midi_build_cc(uint8_t *buf, size_t cap,
                  uint8_t channel, uint8_t number, uint8_t value);

/** Build Program Change message (2 bytes). @thread_safe: yes (pure function) */
int midi_build_program(uint8_t *buf, size_t cap,
                       uint8_t channel, uint8_t program);

/** Build Pitch Bend message (3 bytes). value: -8192..+8191. @thread_safe: yes (pure function) */
int midi_build_pitch_bend(uint8_t *buf, size_t cap,
                          uint8_t channel, int16_t value);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_PACKET_H */
