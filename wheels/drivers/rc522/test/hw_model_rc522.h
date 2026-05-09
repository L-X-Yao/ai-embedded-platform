/*
 * hw_model_rc522.h — datasheet-accurate SPI register model for MFRC522
 *
 * Simulates MFRC522 SPI protocol:
 *   Address byte: bit7 = R(1)/W(0), bits6:1 = register addr, bit0 = 0
 *   Second byte: data (write) or response (read)
 *
 * Pending response pattern: arm a response via hw_model_arm_response()
 * before calling a transceive-based function. The response is injected
 * into the FIFO when the Transceive command (0x0C) is written.
 */
#ifndef HW_MODEL_RC522_H
#define HW_MODEL_RC522_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 64   /* MFRC522 has registers 0x00-0x3F */

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_xfer_count;
static int     hw_cs_state;
static uint8_t hw_addr_latch;
static int     hw_byte_idx;
static bool    hw_is_read;

/* FIFO simulation */
#define HW_FIFO_SIZE 64
static uint8_t hw_fifo[HW_FIFO_SIZE];
static uint8_t hw_fifo_head;
static uint8_t hw_fifo_count;

/* Pending response */
static uint8_t hw_pending_resp[64];
static uint8_t hw_pending_len;
static bool    hw_pending_armed;

static void hw_fifo_push(uint8_t val)
{
    if (hw_fifo_count < HW_FIFO_SIZE) {
        hw_fifo[(hw_fifo_head + hw_fifo_count) % HW_FIFO_SIZE] = val;
        hw_fifo_count++;
    }
}

static uint8_t hw_fifo_pop(void)
{
    if (hw_fifo_count == 0) return 0x00;
    uint8_t val = hw_fifo[hw_fifo_head];
    hw_fifo_head = (uint8_t)((hw_fifo_head + 1) % HW_FIFO_SIZE);
    hw_fifo_count--;
    return val;
}

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_xfer_count = 0;
    hw_cs_state   = 1;
    hw_addr_latch = 0;
    hw_byte_idx   = 0;
    hw_is_read    = false;
    memset(hw_fifo, 0, sizeof(hw_fifo));
    hw_fifo_head  = 0;
    hw_fifo_count = 0;
    memset(hw_pending_resp, 0, sizeof(hw_pending_resp));
    hw_pending_len = 0;
    hw_pending_armed = false;

    /* POR defaults from MFRC522 datasheet */
    hw_regs[0x37] = 0x92;  /* VersionReg: v2.0 */
    hw_regs[0x0A] = 0x00;  /* FIFOLevelReg */
    hw_regs[0x04] = 0x14;  /* ComIrqReg */
    hw_regs[0x06] = 0x00;  /* ErrorReg */
    hw_regs[0x24] = 0x26;  /* ModWidthReg */
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

    if (hw_byte_idx == 0) {
        hw_is_read = (tx & 0x80u) != 0;
        hw_addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
        hw_byte_idx++;
        return 0x00;
    }

    uint8_t ret = 0x00;

    if (hw_is_read) {
        if (hw_addr_latch == 0x09) {
            ret = hw_fifo_pop();
            hw_regs[0x0A] = hw_fifo_count;
        } else if (hw_addr_latch < HW_REG_COUNT) {
            ret = hw_regs[hw_addr_latch];
        }
        if (tx & 0x80u) {
            hw_addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
        }
    } else {
        if (hw_addr_latch == 0x01 && tx == 0x0C && hw_pending_armed) {
            /* Transceive command: inject pending response */
            hw_fifo_head = 0;
            hw_fifo_count = 0;
            for (uint8_t i = 0; i < hw_pending_len; i++) {
                hw_fifo_push(hw_pending_resp[i]);
            }
            hw_regs[0x0A] = hw_fifo_count;
            hw_regs[0x04] = 0x20;  /* RxIRq */
            hw_pending_armed = false;
            hw_regs[hw_addr_latch] = tx;
        } else if (hw_addr_latch == 0x09) {
            hw_fifo_push(tx);
            hw_regs[0x0A] = hw_fifo_count;
        } else if (hw_addr_latch == 0x0A && (tx & 0x80u)) {
            hw_fifo_head = 0;
            hw_fifo_count = 0;
            hw_regs[0x0A] = 0;
        } else if (hw_addr_latch < HW_REG_COUNT) {
            hw_regs[hw_addr_latch] = tx;
        }
    }

    hw_byte_idx++;
    return ret;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* Arm a pending response to be injected when Transceive starts */
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
