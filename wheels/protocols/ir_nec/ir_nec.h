// SPDX-License-Identifier: MIT
/*
 * ir_nec -- NEC infrared remote control protocol encoder/decoder
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef IR_NEC_H
#define IR_NEC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

/* Timing constants (microseconds) */
#define IR_NEC_LEADER_MARK_US       9000u
#define IR_NEC_LEADER_SPACE_US      4500u
#define IR_NEC_REPEAT_SPACE_US      2250u
#define IR_NEC_BIT_MARK_US          562u
#define IR_NEC_BIT_ZERO_SPACE_US    562u
#define IR_NEC_BIT_ONE_SPACE_US     1687u

/* Decode tolerance: +/- 20% */
#define IR_NEC_TOLERANCE_PCT        20u

/* Pulse counts */
#define IR_NEC_PULSE_COUNT          34u   /* leader + 32 data bits + stop */
#define IR_NEC_REPEAT_PULSE_COUNT   3u    /* leader mark/space + stop */

/* ---------- error codes ---------- */

typedef enum {
    IR_NEC_OK           =  0,
    IR_NEC_ERR_PARAM    = -1,
    IR_NEC_ERR_FORMAT   = -2,  /* complement check failed or bad timing */
    IR_NEC_ERR_SHORT    = -3   /* not enough pulses */
} ir_nec_err_t;

/* ---------- data types ---------- */

typedef struct {
    uint8_t address;
    uint8_t command;
} ir_nec_msg_t;

typedef struct {
    uint32_t mark_us;
    uint32_t space_us;
} ir_nec_pulse_t;

/* ---------- API ---------- */

/** Encode a standard NEC message into pulse timing array.
 *  Sets *count = IR_NEC_PULSE_COUNT (34).
 *  @thread_safe: yes @isr_safe: yes */
ir_nec_err_t ir_nec_encode(const ir_nec_msg_t *msg, ir_nec_pulse_t *pulses,
                            uint16_t *count);

/** Decode a pulse timing array into a standard NEC message.
 *  Validates leader timing, data bit timing, and address/command complements.
 *  Timing tolerance: +/- 20%.
 *  @thread_safe: yes @isr_safe: yes */
ir_nec_err_t ir_nec_decode(const ir_nec_pulse_t *pulses, uint16_t count,
                            ir_nec_msg_t *msg);

/** Encode a NEC repeat frame (3 pulses: 9ms mark + 2.25ms space + stop).
 *  Sets *count = IR_NEC_REPEAT_PULSE_COUNT (3).
 *  @thread_safe: yes @isr_safe: yes */
ir_nec_err_t ir_nec_encode_repeat(ir_nec_pulse_t *pulses, uint16_t *count);

/** Check if a pulse array represents a NEC repeat frame.
 *  @thread_safe: yes @isr_safe: yes */
bool ir_nec_is_repeat(const ir_nec_pulse_t *pulses, uint16_t count);

/** Encode an extended NEC message (16-bit address, no address complement).
 *  Sets *count = IR_NEC_PULSE_COUNT (34).
 *  @thread_safe: yes @isr_safe: yes */
ir_nec_err_t ir_nec_encode_extended(uint16_t address, uint8_t command,
                                     ir_nec_pulse_t *pulses, uint16_t *count);

/** Decode an extended NEC message (16-bit address, command + complement).
 *  @thread_safe: yes @isr_safe: yes */
ir_nec_err_t ir_nec_decode_extended(const ir_nec_pulse_t *pulses, uint16_t count,
                                     uint16_t *address, uint8_t *command);

#ifdef __cplusplus
}
#endif

#endif /* IR_NEC_H */
