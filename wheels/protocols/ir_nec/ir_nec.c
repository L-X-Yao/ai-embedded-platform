// SPDX-License-Identifier: MIT
/*
 * ir_nec -- NEC infrared remote control protocol encoder/decoder
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ir_nec.h"

/* ---------- internal helpers ---------- */

/** Check if a timing value is within +/- tolerance_pct of the expected value. */
static bool timing_match(uint32_t actual, uint32_t expected)
{
    uint32_t margin = expected * IR_NEC_TOLERANCE_PCT / 100u;
    uint32_t lo = (expected > margin) ? (expected - margin) : 0u;
    uint32_t hi = expected + margin;
    return (actual >= lo) && (actual <= hi);
}

/** Encode one data byte into pulses, LSB first. Returns number of pulses written. */
static uint16_t encode_byte(uint8_t byte, ir_nec_pulse_t *pulses)
{
    for (uint8_t bit = 0; bit < 8u; bit++) {
        pulses[bit].mark_us = IR_NEC_BIT_MARK_US;
        if ((byte & (uint8_t)(1u << bit)) != 0u) {
            pulses[bit].space_us = IR_NEC_BIT_ONE_SPACE_US;
        } else {
            pulses[bit].space_us = IR_NEC_BIT_ZERO_SPACE_US;
        }
    }
    return 8u;
}

/** Decode 8 data bits from pulses, LSB first. Returns decoded byte. */
static ir_nec_err_t decode_byte(const ir_nec_pulse_t *pulses, uint8_t *out)
{
    uint8_t byte = 0;
    for (uint8_t bit = 0; bit < 8u; bit++) {
        if (!timing_match(pulses[bit].mark_us, IR_NEC_BIT_MARK_US)) {
            return IR_NEC_ERR_FORMAT;
        }
        if (timing_match(pulses[bit].space_us, IR_NEC_BIT_ONE_SPACE_US)) {
            byte |= (uint8_t)(1u << bit);
        } else if (timing_match(pulses[bit].space_us, IR_NEC_BIT_ZERO_SPACE_US)) {
            /* bit is 0, already default */
        } else {
            return IR_NEC_ERR_FORMAT;
        }
    }
    *out = byte;
    return IR_NEC_OK;
}

/* ---------- encode ---------- */

ir_nec_err_t ir_nec_encode(const ir_nec_msg_t *msg, ir_nec_pulse_t *pulses,
                            uint16_t *count)
{
    if (!msg || !pulses || !count) { return IR_NEC_ERR_PARAM; }

    uint16_t idx = 0;

    /* Leader: 9ms mark + 4.5ms space */
    pulses[idx].mark_us  = IR_NEC_LEADER_MARK_US;
    pulses[idx].space_us = IR_NEC_LEADER_SPACE_US;
    idx++;

    /* Address byte (LSB first) */
    idx += encode_byte(msg->address, &pulses[idx]);

    /* Address complement */
    idx += encode_byte((uint8_t)(~msg->address), &pulses[idx]);

    /* Command byte (LSB first) */
    idx += encode_byte(msg->command, &pulses[idx]);

    /* Command complement */
    idx += encode_byte((uint8_t)(~msg->command), &pulses[idx]);

    /* Stop bit: single mark, zero space */
    pulses[idx].mark_us  = IR_NEC_BIT_MARK_US;
    pulses[idx].space_us = 0;
    idx++;

    *count = idx;  /* should be 34 */
    return IR_NEC_OK;
}

/* ---------- decode ---------- */

ir_nec_err_t ir_nec_decode(const ir_nec_pulse_t *pulses, uint16_t count,
                            ir_nec_msg_t *msg)
{
    if (!pulses || !msg) { return IR_NEC_ERR_PARAM; }
    if (count < IR_NEC_PULSE_COUNT) { return IR_NEC_ERR_SHORT; }

    /* Validate leader */
    if (!timing_match(pulses[0].mark_us, IR_NEC_LEADER_MARK_US)) {
        return IR_NEC_ERR_FORMAT;
    }
    if (!timing_match(pulses[0].space_us, IR_NEC_LEADER_SPACE_US)) {
        return IR_NEC_ERR_FORMAT;
    }

    /* Decode 4 bytes */
    uint8_t addr, addr_inv, cmd, cmd_inv;
    ir_nec_err_t err;

    err = decode_byte(&pulses[1], &addr);
    if (err != IR_NEC_OK) { return err; }

    err = decode_byte(&pulses[9], &addr_inv);
    if (err != IR_NEC_OK) { return err; }

    err = decode_byte(&pulses[17], &cmd);
    if (err != IR_NEC_OK) { return err; }

    err = decode_byte(&pulses[25], &cmd_inv);
    if (err != IR_NEC_OK) { return err; }

    /* Verify complements */
    if ((uint8_t)(addr ^ addr_inv) != 0xFFu) { return IR_NEC_ERR_FORMAT; }
    if ((uint8_t)(cmd ^ cmd_inv) != 0xFFu) { return IR_NEC_ERR_FORMAT; }

    msg->address = addr;
    msg->command = cmd;

    return IR_NEC_OK;
}

/* ---------- repeat frame ---------- */

ir_nec_err_t ir_nec_encode_repeat(ir_nec_pulse_t *pulses, uint16_t *count)
{
    if (!pulses || !count) { return IR_NEC_ERR_PARAM; }

    /* Leader: 9ms mark + 2.25ms space */
    pulses[0].mark_us  = IR_NEC_LEADER_MARK_US;
    pulses[0].space_us = IR_NEC_REPEAT_SPACE_US;

    /* Stop bit */
    pulses[1].mark_us  = IR_NEC_BIT_MARK_US;
    pulses[1].space_us = 0;

    /* Third pulse slot unused but set for completeness */
    pulses[2].mark_us  = 0;
    pulses[2].space_us = 0;

    *count = 2u;  /* effectively 2 meaningful pulses */

    return IR_NEC_OK;
}

bool ir_nec_is_repeat(const ir_nec_pulse_t *pulses, uint16_t count)
{
    if (!pulses || count < 2u) { return false; }

    /* Leader: 9ms mark + 2.25ms space (NOT 4.5ms) */
    if (!timing_match(pulses[0].mark_us, IR_NEC_LEADER_MARK_US)) {
        return false;
    }
    if (!timing_match(pulses[0].space_us, IR_NEC_REPEAT_SPACE_US)) {
        return false;
    }

    /* Stop bit mark */
    if (!timing_match(pulses[1].mark_us, IR_NEC_BIT_MARK_US)) {
        return false;
    }

    return true;
}

/* ---------- extended NEC (16-bit address) ---------- */

ir_nec_err_t ir_nec_encode_extended(uint16_t address, uint8_t command,
                                     ir_nec_pulse_t *pulses, uint16_t *count)
{
    if (!pulses || !count) { return IR_NEC_ERR_PARAM; }

    uint16_t idx = 0;

    /* Leader */
    pulses[idx].mark_us  = IR_NEC_LEADER_MARK_US;
    pulses[idx].space_us = IR_NEC_LEADER_SPACE_US;
    idx++;

    /* Address low byte (LSB first) */
    idx += encode_byte((uint8_t)(address & 0xFFu), &pulses[idx]);

    /* Address high byte (LSB first) */
    idx += encode_byte((uint8_t)((address >> 8) & 0xFFu), &pulses[idx]);

    /* Command byte */
    idx += encode_byte(command, &pulses[idx]);

    /* Command complement */
    idx += encode_byte((uint8_t)(~command), &pulses[idx]);

    /* Stop bit */
    pulses[idx].mark_us  = IR_NEC_BIT_MARK_US;
    pulses[idx].space_us = 0;
    idx++;

    *count = idx;  /* should be 34 */
    return IR_NEC_OK;
}

ir_nec_err_t ir_nec_decode_extended(const ir_nec_pulse_t *pulses, uint16_t count,
                                     uint16_t *address, uint8_t *command)
{
    if (!pulses || !address || !command) { return IR_NEC_ERR_PARAM; }
    if (count < IR_NEC_PULSE_COUNT) { return IR_NEC_ERR_SHORT; }

    /* Validate leader */
    if (!timing_match(pulses[0].mark_us, IR_NEC_LEADER_MARK_US)) {
        return IR_NEC_ERR_FORMAT;
    }
    if (!timing_match(pulses[0].space_us, IR_NEC_LEADER_SPACE_US)) {
        return IR_NEC_ERR_FORMAT;
    }

    /* Decode 4 bytes */
    uint8_t addr_lo, addr_hi, cmd, cmd_inv;
    ir_nec_err_t err;

    err = decode_byte(&pulses[1], &addr_lo);
    if (err != IR_NEC_OK) { return err; }

    err = decode_byte(&pulses[9], &addr_hi);
    if (err != IR_NEC_OK) { return err; }

    err = decode_byte(&pulses[17], &cmd);
    if (err != IR_NEC_OK) { return err; }

    err = decode_byte(&pulses[25], &cmd_inv);
    if (err != IR_NEC_OK) { return err; }

    /* Verify command complement */
    if ((uint8_t)(cmd ^ cmd_inv) != 0xFFu) { return IR_NEC_ERR_FORMAT; }

    *address = (uint16_t)(((uint16_t)addr_hi << 8) | (uint16_t)addr_lo);
    *command = cmd;

    return IR_NEC_OK;
}
