// SPDX-License-Identifier: MIT
/*
 * rc522_halt on the wire. A recorder of its own, because the question is what
 * TxModeReg holds at the moment StartSend goes out, and which bytes leave.
 */
#include "test_harness.h"
#include <string.h>
#include "rc522.h"

#define REG_TXMODE   0x12u
#define TX_CRC_EN    0x80u
#define START_SEND   0x80u

static uint8_t regs[64];
static uint8_t fifo[64];
static uint8_t fifo_len;
static uint8_t addr_latch;
static int     byte_idx;
static bool    is_read;

static uint8_t sent[8];          /* the frame of the last StartSend */
static uint8_t sent_len;
static uint8_t txmode_at_send;   /* TxModeReg at the last StartSend */
static bool    card_answers;

static void recorder_reset(void)
{
    memset(regs, 0, sizeof(regs));
    fifo_len = 0;
    sent_len = 0;
    txmode_at_send = 0;
    card_answers = false;
    regs[0x37] = 0x92;   /* VersionReg */
}

static void start_send(void)
{
    sent_len = (fifo_len < sizeof(sent)) ? fifo_len : (uint8_t)sizeof(sent);
    memcpy(sent, fifo, sent_len);
    txmode_at_send = regs[REG_TXMODE];
    fifo_len = 0;
    if (card_answers) {
        fifo[0] = 0x04;
        fifo[1] = 0x00;
        fifo_len = 2;
        regs[0x04] |= 0x30u;   /* RxIRq and IdleIRq */
    } else {
        regs[0x04] |= 0x01u;   /* TimerIRq */
    }
}

static void cs_low(void *ctx)  { (void)ctx; byte_idx = 0; }
static void cs_high(void *ctx) { (void)ctx; }

static uint8_t xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (byte_idx++ == 0) {
        is_read = (tx & 0x80u) != 0;
        addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
        return 0;
    }
    if (is_read) {
        if (addr_latch == 0x0Au)
            return fifo_len;
        if (addr_latch == 0x09u)
            return fifo[0];
        return regs[addr_latch];
    }
    if (addr_latch == 0x09u) {
        if (fifo_len < sizeof(fifo))
            fifo[fifo_len++] = tx;
    } else if (addr_latch == 0x0Au) {
        if (tx & 0x80u)
            fifo_len = 0;
    } else if (addr_latch == 0x04u) {
        regs[0x04] &= (uint8_t)~tx;   /* Set1 = 0: clear the marked bits */
    } else {
        regs[addr_latch] = tx;
        if (addr_latch == 0x0Du && (tx & START_SEND))
            start_send();
    }
    return 0;
}

static const rc522_spi_t spi = {
    .cs_low = cs_low, .cs_high = cs_high, .xfer = xfer,
    .delay_ms = NULL, .ctx = NULL,
};

TEST(test_hlta_goes_out_with_crc)
{
    recorder_reset();
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &spi), RC522_OK);

    ASSERT_EQ(rc522_halt(&dev), RC522_OK);

    /* The chip adds CRC_A itself when TxCRCEn is set (data sheet p.49) */
    ASSERT_EQ(sent_len, 2);
    ASSERT_EQ_HEX(sent[0], 0x50);
    ASSERT_EQ_HEX(sent[1], 0x00);
    ASSERT_EQ_HEX(txmode_at_send & TX_CRC_EN, TX_CRC_EN);
}

TEST(test_txmode_is_restored)
{
    recorder_reset();
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &spi), RC522_OK);
    regs[REG_TXMODE] = 0x08u;   /* InvMod: the rest of the register is not ours */

    ASSERT_EQ(rc522_halt(&dev), RC522_OK);
    ASSERT_EQ_HEX(regs[REG_TXMODE], 0x08);

    /* REQA is a short frame and must not get a CRC */
    regs[REG_TXMODE] = 0x00u;
    ASSERT_EQ(rc522_halt(&dev), RC522_OK);
    card_answers = true;
    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
    ASSERT_EQ_HEX(sent[0], 0x26);
    ASSERT_EQ_HEX(txmode_at_send & TX_CRC_EN, 0);
}

TEST(test_reply_means_not_acknowledged)
{
    recorder_reset();
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &spi), RC522_OK);

    card_answers = true;
    ASSERT_EQ(rc522_halt(&dev), RC522_ERR_NACK);
    ASSERT_EQ_HEX(regs[REG_TXMODE] & TX_CRC_EN, 0);
}

TEST_MAIN("halt_rc522")
    RUN(test_hlta_goes_out_with_crc);
    RUN(test_txmode_is_restored);
    RUN(test_reply_means_not_acknowledged);
TEST_MAIN_END()
