// SPDX-License-Identifier: MIT
/*
 * hw_model_rc522.h — SPI register model of the MFRC522, after NXP data sheet
 * rev 3.9 (27 April 2016)
 *
 * Simulates MFRC522 SPI protocol (section 8.1.2):
 *   Address byte: bit7 = R(1)/W(0), bits6:1 = register addr, bit0 = 0
 *   Second byte: data (write) or response (read)
 *
 * Pending response pattern: arm a response via hw_model_arm_response()
 * before calling a transceive-based function. It stands for a card in the
 * field, so it reaches the FIFO only when the chip really transmits: Transceive
 * active, StartSend written, a TX driver on and the receiver not switched off.
 * With no card, or no field, the timer runs out instead if TAuto is set.
 *
 * Time passes only when the host polls: what happens on the air ends on the
 * HW_AIR_POLLS-th read of ComIrqReg, a self-test on the HW_SELF_TEST_POLLS-th
 * read of FIFOLevelReg. Until then a reader sees the chip still busy.
 */
#ifndef HW_MODEL_RC522_H
#define HW_MODEL_RC522_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 64   /* MFRC522 has registers 0x00-0x3F */
#define HW_AIR_POLLS       3
#define HW_SELF_TEST_POLLS 3

/* Reset values, tables of section 9.3, pp.38-69. The manual gives xxh for
 * DivIrqReg's upper bits, FIFODataReg, CollReg, TCounterValReg, TestBusReg,
 * VersionReg, TestDAC1Reg, TestDAC2Reg and TestADCReg: those read 00h here. */
static const uint8_t hw_reset_values[HW_REG_COUNT] = {
    0x00, 0x20, 0x80, 0x00, 0x14, 0x00, 0x00, 0x21,  /* 00h */
    0x00, 0x00, 0x00, 0x08, 0x10, 0x00, 0x00, 0x00,  /* 08h */
    0x00, 0x3F, 0x00, 0x00, 0x80, 0x00, 0x10, 0x84,  /* 10h */
    0x84, 0x4D, 0x00, 0x00, 0x62, 0x00, 0x00, 0xEB,  /* 18h */
    0x00, 0xFF, 0xFF, 0x88, 0x26, 0x87, 0x48, 0x88,  /* 20h */
    0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 28h */
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x40, 0x00,  /* 30h */
    0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x03, 0x00,  /* 38h */
};

/* Self-test results, section 16.1.1, p.82. The version 2.0 list prints two
 * values as 061h and 03Bh. */
static const uint8_t hw_selftest_v1[64] = {
    0x00, 0xC6, 0x37, 0xD5, 0x32, 0xB7, 0x57, 0x5C,
    0xC2, 0xD8, 0x7C, 0x4D, 0xD9, 0x70, 0xC7, 0x73,
    0x10, 0xE6, 0xD2, 0xAA, 0x5E, 0xA1, 0x3E, 0x5A,
    0x14, 0xAF, 0x30, 0x61, 0xC9, 0x70, 0xDB, 0x2E,
    0x64, 0x22, 0x72, 0xB5, 0xBD, 0x65, 0xF4, 0xEC,
    0x22, 0xBC, 0xD3, 0x72, 0x35, 0xCD, 0xAA, 0x41,
    0x1F, 0xA7, 0xF3, 0x53, 0x14, 0xDE, 0x7E, 0x02,
    0xD9, 0x0F, 0xB5, 0x5E, 0x25, 0x1D, 0x29, 0x79,
};

static const uint8_t hw_selftest_v2[64] = {
    0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
    0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
    0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
    0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
    0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
    0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
    0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
    0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F,
};

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_xfer_count;
static int     hw_cs_state;
static uint8_t hw_addr_latch;
static int     hw_byte_idx;
static bool    hw_is_read;
static int     hw_addr_errors;   /* address bytes with bit 0 set (p.11) */

/* FIFO simulation */
#define HW_FIFO_SIZE 64
static uint8_t hw_fifo[HW_FIFO_SIZE];
static uint8_t hw_fifo_head;
static uint8_t hw_fifo_count;

/* 25-byte internal buffer, which a soft reset leaves alone (p.73) */
#define HW_MEM_SIZE 25
static uint8_t hw_mem[HW_MEM_SIZE];
static bool    hw_reset_since_self_test;

/* Pending response */
static uint8_t hw_pending_resp[64];
static uint8_t hw_pending_len;
static bool    hw_pending_armed;

/* Last frame sent, and the ErrorReg bits the next reception raises */
static uint8_t hw_tx_frame[HW_FIFO_SIZE];
static uint8_t hw_tx_len;
static uint8_t hw_tx_last_bits;
static int     hw_tx_count;
static uint8_t hw_rx_errors;

/* Reads left until the running exchange or self-test ends, 0 when none runs */
static int     hw_air_polls;
static int     hw_self_test_polls;

/* Faults a test can put into the self-test result */
static bool    hw_self_test_hangs;
static bool    hw_self_test_all_zero;
static bool    hw_self_test_corrupt;

static void hw_fifo_flush(void)
{
    hw_fifo_head  = 0;
    hw_fifo_count = 0;
    hw_regs[0x0A] = 0;
}

static void hw_fifo_push(uint8_t val)
{
    if (hw_fifo_count < HW_FIFO_SIZE) {
        hw_fifo[(hw_fifo_head + hw_fifo_count) % HW_FIFO_SIZE] = val;
        hw_fifo_count++;
        hw_regs[0x0A] = hw_fifo_count;
    } else {
        /* p.41: BufferOvfl when a write finds the FIFO already full; table 30
         * sets ErrIRq for any error bit */
        hw_regs[0x06] |= 0x10u;
        hw_regs[0x04] |= 0x02u;
    }
}

static uint8_t hw_fifo_pop(void)
{
    if (hw_fifo_count == 0) return 0x00;
    uint8_t val = hw_fifo[hw_fifo_head];
    hw_fifo_head = (uint8_t)((hw_fifo_head + 1) % HW_FIFO_SIZE);
    hw_fifo_count--;
    hw_regs[0x0A] = hw_fifo_count;
    return val;
}

/* VersionReg is read only and fixed by the chip, so a reset keeps it */
static void hw_load_reset_values(void)
{
    uint8_t version = hw_regs[0x37];
    memcpy(hw_regs, hw_reset_values, sizeof(hw_regs));
    hw_regs[0x37] = version;
    hw_fifo_flush();
}

static void hw_model_reset(void)
{
    hw_regs[0x37] = 0x92;  /* VersionReg: v2.0 */
    hw_load_reset_values();
    hw_xfer_count = 0;
    hw_cs_state   = 1;
    hw_addr_latch = 0;
    hw_byte_idx   = 0;
    hw_is_read    = false;
    hw_addr_errors = 0;
    memset(hw_fifo, 0, sizeof(hw_fifo));
    memset(hw_mem, 0xA5, sizeof(hw_mem));  /* unknown after power on */
    hw_reset_since_self_test = false;
    memset(hw_pending_resp, 0, sizeof(hw_pending_resp));
    hw_pending_len = 0;
    hw_pending_armed = false;
    memset(hw_tx_frame, 0, sizeof(hw_tx_frame));
    hw_tx_len = 0;
    hw_tx_last_bits = 0;
    hw_tx_count = 0;
    hw_rx_errors = 0;
    hw_air_polls = 0;
    hw_self_test_polls = 0;
    hw_self_test_hangs = false;
    hw_self_test_all_zero = false;
    hw_self_test_corrupt = false;
}

/* A command that ends by itself falls back to Idle and sets IdleIRq (p.40) */
static void hw_command_done(void)
{
    hw_regs[0x01] &= 0xF0u;
    hw_regs[0x04] |= 0x10u;
}

/* p.71: with 25 bytes in the FIFO Mem stores them, with an empty FIFO it hands
 * the buffer back. p.70: otherwise it waits for the rest of its arguments. */
static void hw_mem_step(void)
{
    if (hw_fifo_count >= HW_MEM_SIZE) {
        for (uint8_t i = 0; i < HW_MEM_SIZE; i++) hw_mem[i] = hw_fifo_pop();
        hw_command_done();
    } else if (hw_fifo_count == 0) {
        for (uint8_t i = 0; i < HW_MEM_SIZE; i++) hw_fifo_push(hw_mem[i]);
        hw_command_done();
    }
}

/* p.82 gives the result of the whole procedure only. Whatever a real chip does
 * when a step is left out, it is not documented, so here it is not the list. */
static void hw_self_test(void)
{
    bool clean = hw_reset_since_self_test && hw_fifo_count == 1 &&
                 hw_fifo[hw_fifo_head] == 0x00;
    for (uint8_t i = 0; i < HW_MEM_SIZE; i++) {
        if (hw_mem[i] != 0x00) clean = false;
    }
    const uint8_t *list = (hw_regs[0x37] == 0x91) ? hw_selftest_v1
                                                  : hw_selftest_v2;
    hw_fifo_flush();
    for (uint8_t i = 0; i < 64; i++) {
        uint8_t val = clean ? list[i] : (uint8_t)~list[i];
        hw_fifo_push(hw_self_test_all_zero ? 0x00 : val);
    }
    if (hw_self_test_corrupt) hw_fifo[5] ^= 0x01u;
    hw_reset_since_self_test = false;
}

/* StartSend under Transceive: the FIFO goes out on the air (p.72) */
static void hw_transmit(void)
{
    hw_tx_len = 0;
    while (hw_fifo_count > 0) hw_tx_frame[hw_tx_len++] = hw_fifo_pop();
    hw_tx_last_bits = hw_regs[0x0D] & 0x07u;
    hw_tx_count++;
    hw_regs[0x04] |= 0x40u;  /* TxIRq */

    /* p.41: these errors clear in the receiver start-up phase */
    hw_regs[0x06] &= (uint8_t)~0x0Fu;
    hw_air_polls = HW_AIR_POLLS;
}

/* The exchange on the air ends: a card answered, or the timer ran out */
static void hw_air_done(void)
{
    bool field = (hw_regs[0x14] & 0x03u) != 0;   /* p.50: Tx1RFEn, Tx2RFEn */
    bool rx_on = (hw_regs[0x01] & 0x20u) == 0;   /* p.38: RcvOff */
    if (hw_pending_armed && field && rx_on) {
        hw_pending_armed = false;
        for (uint8_t i = 0; i < hw_pending_len; i++) {
            hw_fifo_push(hw_pending_resp[i]);
        }
        hw_regs[0x06] |= hw_rx_errors;
        if (hw_rx_errors != 0) hw_regs[0x04] |= 0x02u;  /* ErrIRq */
        hw_rx_errors = 0;
        hw_regs[0x04] |= 0x20u;  /* RxIRq; the 5th bit stopped the timer, p.61 */
    } else if ((hw_regs[0x2A] & 0x80u) != 0 &&
               (hw_regs[0x2C] != 0 || hw_regs[0x2D] != 0)) {
        /* p.61: TAuto starts the timer at the end of the transmission, and
         * p.30: TimerIRq is set when it decrements from 1 to 0 */
        hw_regs[0x04] |= 0x01u;
    }
}

static void hw_write_command(uint8_t val)
{
    uint8_t cmd = val & 0x0Fu;

    /* p.71: NoCmdChange leaves the running command alone */
    if (cmd == 0x07u) {
        hw_regs[0x01] = (uint8_t)((val & 0x30u) | (hw_regs[0x01] & 0x0Fu));
        return;
    }
    hw_regs[0x01] = val & 0x3Fu;  /* bits 7:6 are reserved */

    /* p.70: "Each command can be interrupted by the host writing a new command
     * code" */
    hw_air_polls = 0;
    hw_self_test_polls = 0;

    switch (cmd) {
    case 0x00u:  /* Idle: the host starting it sets no IdleIRq, p.40 */
    case 0x0Cu:  /* Transceive: waits for StartSend, p.70 */
    case 0x02u:  /* Generate RandomID, Transmit, Receive and MFAuthent: */
    case 0x04u:  /* the driver has no use for them */
    case 0x08u:
    case 0x0Eu:
        break;
    case 0x01u:
        hw_mem_step();
        break;
    case 0x03u:
        /* p.71: SelfTest[3:0] decides between a CRC and the self test */
        if ((hw_regs[0x36] & 0x0Fu) == 0x09u && !hw_self_test_hangs) {
            hw_self_test_polls = HW_SELF_TEST_POLLS;
        }
        break;
    case 0x0Fu:
        /* p.73: registers back to reset, internal buffer unchanged */
        hw_load_reset_values();
        hw_reset_since_self_test = true;
        break;
    default:
        hw_command_done();  /* p.40: an unknown command goes idle */
        break;
    }
}

static void hw_write_reg(uint8_t reg, uint8_t val)
{
    switch (reg) {
    case 0x01:
        hw_write_command(val);
        break;
    case 0x04:
        /* p.40: Set1 says whether the marked bits are set or cleared */
        if (val & 0x80u) {
            hw_regs[0x04] |= (uint8_t)(val & 0x7Fu);
        } else {
            hw_regs[0x04] &= (uint8_t)~val;
        }
        break;
    case 0x09:
        hw_fifo_push(val);
        if ((hw_regs[0x01] & 0x0Fu) == 0x01u) hw_mem_step();
        break;
    case 0x0A:
        /* p.44: only FlushBuffer can be written; p.30: it clears BufferOvfl */
        if (val & 0x80u) {
            hw_fifo_flush();
            hw_regs[0x06] &= (uint8_t)~0x10u;
        }
        break;
    case 0x0D:
        /* p.46: StartSend is write only, and bit 3 reserved */
        hw_regs[0x0D] = val & 0x77u;
        if ((val & 0x80u) && (hw_regs[0x01] & 0x0Fu) == 0x0Cu) hw_transmit();
        break;
    case 0x06:  /* ErrorReg */
    case 0x07:  /* Status1Reg */
    case 0x21:  /* CRCResultReg */
    case 0x22:
    case 0x2E:  /* TCounterValReg */
    case 0x2F:
    case 0x35:  /* TestBusReg */
    case 0x37:  /* VersionReg */
    case 0x3B:  /* TestADCReg */
        break;  /* R access, table 19, p.35 */
    default:
        hw_regs[reg] = val;
        /* p.82 starts from a reset chip, not from one configured after it */
        if (reg != 0x36u) hw_reset_since_self_test = false;
        break;
    }
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx) { (void)ctx; hw_cs_state = 0; hw_byte_idx = 0; }

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    hw_xfer_count++;

    if (hw_cs_state != 0) return 0x00;  /* not selected */

    if (hw_byte_idx == 0) {
        hw_is_read = (tx & 0x80u) != 0;
        hw_addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
        if (tx & 0x01u) hw_addr_errors++;
        hw_byte_idx++;
        return 0x00;
    }

    uint8_t ret = 0x00;

    if (hw_is_read) {
        if (hw_addr_latch == 0x09) {
            ret = hw_fifo_pop();
        } else {
            if (hw_addr_latch == 0x04 && hw_air_polls > 0 &&
                --hw_air_polls == 0) {
                hw_air_done();
            }
            if (hw_addr_latch == 0x0A && hw_self_test_polls > 0 &&
                --hw_self_test_polls == 0) {
                hw_self_test();
            }
            ret = hw_regs[hw_addr_latch];
        }
        if (tx & 0x80u) {
            hw_addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
            if (tx & 0x01u) hw_addr_errors++;
        }
    } else {
        hw_write_reg(hw_addr_latch, tx);
    }

    hw_byte_idx++;
    return ret;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* Arm a pending response: a card that answers the next transmission */
__attribute__((unused))
static void hw_model_arm_response(const uint8_t *data, uint8_t len)
{
    memcpy(hw_pending_resp, data, len);
    hw_pending_len = len;
    hw_pending_armed = true;
}

/* Convenience: arm ATQA response */
__attribute__((unused))
static void hw_model_inject_atqa(uint8_t atqa_lo, uint8_t atqa_hi)
{
    uint8_t data[2] = {atqa_lo, atqa_hi};
    hw_model_arm_response(data, 2);
}

/* Convenience: arm UID response (4 bytes + auto BCC) */
__attribute__((unused))
static void hw_model_inject_uid(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    uint8_t data[5] = {b0, b1, b2, b3, (uint8_t)(b0 ^ b1 ^ b2 ^ b3)};
    hw_model_arm_response(data, 5);
}

#endif /* HW_MODEL_RC522_H */
