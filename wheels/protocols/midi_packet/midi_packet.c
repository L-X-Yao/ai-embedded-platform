// SPDX-License-Identifier: MIT
/*
 * midi_packet — MIDI message encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "midi_packet.h"
#include <string.h>

/* --- Helpers --- */

static bool is_status_byte(uint8_t b) { return (b & 0x80) != 0; }
static bool is_realtime(uint8_t b)    { return b >= 0xF8; }

static uint8_t data_bytes_expected(uint8_t status)
{
    uint8_t hi = status & 0xF0;
    switch (hi) {
    case 0x80: return 2;   /* Note Off */
    case 0x90: return 2;   /* Note On */
    case 0xA0: return 2;   /* Poly Aftertouch */
    case 0xB0: return 2;   /* Control Change */
    case 0xC0: return 1;   /* Program Change */
    case 0xD0: return 1;   /* Channel Pressure */
    case 0xE0: return 2;   /* Pitch Bend */
    default:   return 0;
    }
}

static void emit_msg(uint8_t status, const uint8_t *data, midi_msg_t *out)
{
    uint8_t hi = status & 0xF0;
    out->channel = status & 0x0F;

    switch (hi) {
    case MIDI_NOTE_OFF:
        out->type = MIDI_MSG_NOTE_OFF;
        out->d.note.note     = data[0];
        out->d.note.velocity = data[1];
        break;
    case MIDI_NOTE_ON:
        if (data[1] == 0) {
            out->type = MIDI_MSG_NOTE_OFF;   /* velocity 0 = Note Off */
        } else {
            out->type = MIDI_MSG_NOTE_ON;
        }
        out->d.note.note     = data[0];
        out->d.note.velocity = data[1];
        break;
    case MIDI_CONTROL_CHANGE:
        out->type = MIDI_MSG_CONTROL_CHANGE;
        out->d.cc.number = data[0];
        out->d.cc.value  = data[1];
        break;
    case MIDI_PROGRAM_CHANGE:
        out->type = MIDI_MSG_PROGRAM_CHANGE;
        out->d.pc.program = data[0];
        break;
    case MIDI_PITCH_BEND:
        out->type = MIDI_MSG_PITCH_BEND;
        out->d.pitch_bend.value =
            (int16_t)(((int16_t)data[1] << 7 | data[0]) - 8192);
        break;
    default:
        out->type = MIDI_MSG_NONE;
        break;
    }
}

/* --- Parser --- */

void midi_parser_init(midi_parser_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
}

bool midi_parse(midi_parser_t *p, uint8_t byte, midi_msg_t *out)
{
    if (!p || !out) return false;

    out->type = MIDI_MSG_NONE;

    /* Real-time bytes (0xF8-0xFF) are ignored — they can appear anywhere
     * in the stream and must not disrupt message parsing. */
    if (is_realtime(byte)) return false;

    /* SysEx handling */
    if (byte == MIDI_SYSEX_START) {
        p->in_sysex        = true;
        p->sysex_len       = 0;
        p->sysex_overflow  = false;
        p->running_status  = 0;
        p->buf_pos         = 0;
        p->expected        = 0;
        return false;
    }

    if (p->in_sysex) {
        if (byte == MIDI_SYSEX_END) {
            p->in_sysex     = false;
            out->type       = MIDI_MSG_SYSEX;
            out->channel    = 0;
            out->d.sysex.data = p->sysex_buf;
            out->d.sysex.len  = p->sysex_len;
            return true;
        }
        /* Accumulate SysEx data bytes (7-bit) */
        if (!is_status_byte(byte)) {
            if (p->sysex_len < MIDI_SYSEX_MAX) {
                p->sysex_buf[p->sysex_len++] = byte;
            } else {
                p->sysex_overflow = true;
            }
        }
        return false;
    }

    /* Channel voice status byte */
    if (is_status_byte(byte)) {
        if (byte >= 0xF0) {
            /* System common (other than SysEx start handled above) — reset */
            p->running_status = 0;
            p->buf_pos   = 0;
            p->expected  = 0;
            return false;
        }
        p->running_status = byte;
        p->expected = data_bytes_expected(byte);
        p->buf_pos  = 0;
        return false;
    }

    /* Data byte — need an active status */
    if (p->running_status == 0) return false;

    p->buf[p->buf_pos++] = byte;

    if (p->buf_pos >= p->expected) {
        emit_msg(p->running_status, p->buf, out);
        p->buf_pos = 0;  /* ready for next message (running status) */
        return true;
    }

    return false;
}

/* --- Builders --- */

static bool validate_channel(uint8_t ch) { return ch <= 15; }
static bool validate_7bit(uint8_t v)     { return v <= 127; }

int midi_build_note_on(uint8_t *buf, size_t cap,
                       uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (!buf) return MIDI_PACKET_ERR_PARAM;
    if (!validate_channel(channel) || !validate_7bit(note) ||
        !validate_7bit(velocity))
        return MIDI_PACKET_ERR_RANGE;
    if (cap < 3) return MIDI_PACKET_ERR_SIZE;

    buf[0] = (uint8_t)(MIDI_NOTE_ON | channel);
    buf[1] = note;
    buf[2] = velocity;
    return 3;
}

int midi_build_note_off(uint8_t *buf, size_t cap,
                        uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (!buf) return MIDI_PACKET_ERR_PARAM;
    if (!validate_channel(channel) || !validate_7bit(note) ||
        !validate_7bit(velocity))
        return MIDI_PACKET_ERR_RANGE;
    if (cap < 3) return MIDI_PACKET_ERR_SIZE;

    buf[0] = (uint8_t)(MIDI_NOTE_OFF | channel);
    buf[1] = note;
    buf[2] = velocity;
    return 3;
}

int midi_build_cc(uint8_t *buf, size_t cap,
                  uint8_t channel, uint8_t number, uint8_t value)
{
    if (!buf) return MIDI_PACKET_ERR_PARAM;
    if (!validate_channel(channel) || !validate_7bit(number) ||
        !validate_7bit(value))
        return MIDI_PACKET_ERR_RANGE;
    if (cap < 3) return MIDI_PACKET_ERR_SIZE;

    buf[0] = (uint8_t)(MIDI_CONTROL_CHANGE | channel);
    buf[1] = number;
    buf[2] = value;
    return 3;
}

int midi_build_program(uint8_t *buf, size_t cap,
                       uint8_t channel, uint8_t program)
{
    if (!buf) return MIDI_PACKET_ERR_PARAM;
    if (!validate_channel(channel) || !validate_7bit(program))
        return MIDI_PACKET_ERR_RANGE;
    if (cap < 2) return MIDI_PACKET_ERR_SIZE;

    buf[0] = (uint8_t)(MIDI_PROGRAM_CHANGE | channel);
    buf[1] = program;
    return 2;
}

int midi_build_pitch_bend(uint8_t *buf, size_t cap,
                          uint8_t channel, int16_t value)
{
    if (!buf) return MIDI_PACKET_ERR_PARAM;
    if (!validate_channel(channel)) return MIDI_PACKET_ERR_RANGE;
    if (value < -8192 || value > 8191) return MIDI_PACKET_ERR_RANGE;
    if (cap < 3) return MIDI_PACKET_ERR_SIZE;

    uint16_t raw = (uint16_t)(value + 8192);
    buf[0] = (uint8_t)(MIDI_PITCH_BEND | channel);
    buf[1] = (uint8_t)(raw & 0x7F);          /* LSB */
    buf[2] = (uint8_t)((raw >> 7) & 0x7F);   /* MSB */
    return 3;
}
