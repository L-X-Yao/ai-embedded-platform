// SPDX-License-Identifier: MIT
/*
 * test_hw_model_enc28j60.c -- tests using ENC28J60 hardware register model
 *
 * Validates the enc28j60 driver against a register-accurate model with
 * SPI command protocol (RCR/WCR/BFS/BFC/RBM/WBM/SRC), banked register
 * file, 8KB buffer memory, and PHY indirect access.
 *
 * Tests: init sequence, bank selection, register read/write per bank,
 * buffer memory write + readback, PHY indirect access, soft reset,
 * bit field set/clear, TX flow, read-only register protection.
 */

#include "test_harness.h"
#include "hw_model_enc28j60.h"
#include "../enc28j60.h"

static enc28j60_model_t model;

static const enc28j60_spi_t model_spi = {
    .cs_low   = enc28j60_model_cs_low,
    .cs_high  = enc28j60_model_cs_high,
    .xfer     = enc28j60_model_xfer,
    .delay_ms = enc28j60_model_delay_ms,
    .ctx      = &model,
};

static const uint8_t test_mac[6] = {0x02, 0x00, 0x00, 0x11, 0x22, 0x33};

static enc28j60_t dev;

static void setup(void)
{
    enc28j60_model_init(&model);
    memset(&dev, 0, sizeof(dev));
}

static void setup_and_init(void)
{
    setup();
    enc28j60_init(&dev, &model_spi, test_mac);
    /* Reset captured TX data after init */
    model.tx_captured_len = 0;
    model.tx_econ1_read_count = 0;
}

/* --- 1. Init sequence: soft reset, buffer pointers, MAC init --- */

TEST(test_init_sequence)
{
    setup();
    enc28j60_err_t err = enc28j60_init(&dev, &model_spi, test_mac);
    ASSERT_EQ(err, ENC28J60_OK);

    /* Soft reset must have occurred */
    ASSERT_TRUE(model.reset_count > 0);

    /* Delay must have been called after reset */
    ASSERT_TRUE(model.delay_count > 0);

    /* RX buffer start (bank 0): ERXSTL/H = 0x0000 */
    ASSERT_EQ(model.regs[0][ENC_ERXSTL], 0x00);
    ASSERT_EQ(model.regs[0][ENC_ERXSTH], 0x00);

    /* RX buffer end: ERXNDL/H = 0x19FF */
    ASSERT_EQ(model.regs[0][ENC_ERXNDL], 0xFF);
    ASSERT_EQ(model.regs[0][ENC_ERXNDH], 0x19);

    /* MAC enable (bank 2): MACON1.MARXEN set */
    ASSERT_TRUE(model.regs[2][ENC_MACON1] & 0x01U);

    /* MACON3: PADCFG0 + TXCRCEN + FRMLNEN */
    ASSERT_TRUE(model.regs[2][ENC_MACON3] & 0x32U);

    /* MAC address written (bank 3) */
    ASSERT_EQ(model.regs[3][ENC_MAADR1], 0x02);
    ASSERT_EQ(model.regs[3][ENC_MAADR2], 0x00);
    ASSERT_EQ(model.regs[3][ENC_MAADR5], 0x22);
    ASSERT_EQ(model.regs[3][ENC_MAADR6], 0x33);

    /* RXEN enabled (common register ECON1) */
    ASSERT_TRUE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);
}

/* --- 2. Bank selection via ECON1[1:0] --- */

TEST(test_bank_selection)
{
    setup_and_init();

    /* After init, the driver may be in any bank. Force to bank 0 by
     * reading a bank-0 register (ERDPTL via a register read) */
    enc28j60_get_revision(&dev);  /* reads EREVID (bank 3) */
    ASSERT_EQ(dev.bank, 3);

    /* has_packet reads EPKTCNT (bank 1) */
    enc28j60_model_set_pktcnt(&model, 0);
    enc28j60_has_packet(&dev);
    ASSERT_EQ(dev.bank, 1);

    /* Verify the model's bank tracking agrees with the driver */
    ASSERT_EQ(model.current_bank, 1);
}

/* --- 3. Register read/write per bank --- */

TEST(test_register_rw_per_bank)
{
    setup_and_init();

    /* Bank 0: read back ERXSTL which init set to 0x00 */
    ASSERT_EQ(model.regs[0][ENC_ERXSTL], 0x00);

    /* Bank 1: ERXFCON should have been set during init */
    /* UCEN(0x80) | CRCEN(0x20) | BCEN(0x01) = 0xA1 */
    ASSERT_EQ(model.regs[1][ENC_ERXFCON], 0xA1);

    /* Bank 2: MABBIPG = 0x12 (set during init) */
    ASSERT_EQ(model.regs[2][ENC_MABBIPG], 0x12);

    /* Bank 3: MAC address registers verified in test_init_sequence */
    ASSERT_EQ(model.regs[3][ENC_MAADR1], 0x02);
}

/* --- 4. Buffer memory write + read back --- */

TEST(test_buffer_write_read)
{
    setup_and_init();

    /* The driver writes frame to TX buffer via WBM.
     * We test by sending a frame and verifying it appears in model.buffer. */
    uint8_t frame[32];
    for (int i = 0; i < 32; i++) frame[i] = (uint8_t)(0xA0 + i);

    model.tx_complete_after = 1;  /* TX completes immediately */
    enc28j60_err_t err = enc28j60_send(&dev, frame, 32);
    ASSERT_EQ(err, ENC28J60_OK);

    /* tx_captured should have: control byte (0x00) + 32 frame bytes */
    ASSERT_TRUE(model.tx_captured_len >= 33);
    ASSERT_EQ(model.tx_captured[0], 0x00);  /* per-packet control byte */
    ASSERT_EQ(model.tx_captured[1], 0xA0);  /* first frame byte */
    ASSERT_EQ(model.tx_captured[32], 0xBF); /* last frame byte */

    /* Verify buffer memory also has the data at TXSTART offset */
    ASSERT_EQ(model.buffer[ENC_TXSTART], 0x00);      /* control byte */
    ASSERT_EQ(model.buffer[ENC_TXSTART + 1], 0xA0);  /* frame[0] */
}

/* --- 5. PHY register indirect access --- */

TEST(test_phy_indirect_read)
{
    setup_and_init();

    /* Set a PHY register value (e.g., PHSTAT1 at PHY addr 0x01) */
    enc28j60_model_set_phy(&model, 0x01, 0x1234);

    /* To read a PHY register the driver would:
     *   1. Write MIREGADR (bank 2, 0x14) with PHY register address
     *   2. Set MICMD.MIIRD (bank 2, 0x12)
     *   3. Poll MISTAT.BUSY (bank 3, 0x0A) until clear
     *   4. Read MIRDL/H (bank 2, 0x18/0x19)
     *
     * We simulate this manually since the driver doesn't expose a
     * PHY read API directly (it's done internally during init/config). */

    /* Step 1: write MIREGADR with PHY address 0x01 */
    /* Switch to bank 2 by setting ECON1[1:0] = 2 */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFC | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_BSEL0 | ENC_ECON1_BSEL1);
    model_spi.cs_high(model_spi.ctx);

    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFS | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_BSEL1);  /* bank 2 */
    model_spi.cs_high(model_spi.ctx);

    /* Write MIREGADR = 0x01 */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_WCR | ENC_MIREGADR);
    model_spi.xfer(model_spi.ctx, 0x01);
    model_spi.cs_high(model_spi.ctx);

    /* Step 2: write MICMD = MIIRD */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_WCR | ENC_MICMD);
    model_spi.xfer(model_spi.ctx, ENC_MICMD_MIIRD);
    model_spi.cs_high(model_spi.ctx);

    /* The model immediately completes the PHY read, so MIRDL/H
     * should now contain 0x1234 */
    ASSERT_EQ(model.regs[2][ENC_MIRDL], 0x34);
    ASSERT_EQ(model.regs[2][ENC_MIRDH], 0x12);
}

/* --- 6. Soft reset restores defaults --- */

TEST(test_soft_reset_restores_defaults)
{
    setup_and_init();

    /* Modify some registers: ERXFCON (bank 1) was set to 0xA1 by init */
    ASSERT_EQ(model.regs[1][ENC_ERXFCON], 0xA1);
    /* MAC address (bank 3) was written */
    ASSERT_EQ(model.regs[3][ENC_MAADR1], 0x02);

    /* Now perform a soft reset through the model directly */
    int prev_reset = model.reset_count;
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_SRC);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_EQ(model.reset_count, prev_reset + 1);

    /* After reset, modified registers should be back to defaults (0) */
    ASSERT_EQ(model.regs[1][ENC_ERXFCON], 0x00);
    ASSERT_EQ(model.regs[3][ENC_MAADR1], 0x00);
    /* Bank should be reset to 0 */
    ASSERT_EQ(model.current_bank, 0);
    /* EREVID should still be at default 0x06 */
    ASSERT_EQ(model.regs[3][ENC_EREVID], 0x06);
    /* ESTAT.CLKRDY is unaffected by an SRC: init polled it set */
    ASSERT_TRUE(model.common[ENC_ESTAT] & ENC_ESTAT_CLKRDY);
}

/* --- 7. Bit field set/clear operations --- */

TEST(test_bit_field_ops)
{
    setup_and_init();

    /* ECON1 is a common register. Clear RXEN via BFC, then set it via BFS. */
    /* First, verify RXEN is currently set (from init) */
    ASSERT_TRUE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);

    /* BFC: clear RXEN */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFC | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_RXEN);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_FALSE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);

    /* BFS: set RXEN back */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFS | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_RXEN);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_TRUE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);

    /* BFS should only set bits, not clear others.
     * Set TXRTS while RXEN is still set. */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFS | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_TXRTS);
    model_spi.cs_high(model_spi.ctx);

    /* Both RXEN and TXRTS should be set */
    ASSERT_TRUE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);
    ASSERT_TRUE(model.common[ENC_ECON1] & ENC_ECON1_TXRTS);
}

/* --- 8. TX flow: write to buffer, set pointers, trigger TXRTS --- */

TEST(test_tx_flow)
{
    setup_and_init();

    uint8_t frame[64];
    memset(frame, 0xBB, sizeof(frame));

    model.tx_complete_after = 1;
    enc28j60_err_t err = enc28j60_send(&dev, frame, 64);
    ASSERT_EQ(err, ENC28J60_OK);

    /* TX start pointer (bank 0) should be TXSTART */
    ASSERT_EQ(model.regs[0][ENC_ETXSTL], (uint8_t)(ENC_TXSTART & 0xFF));
    ASSERT_EQ(model.regs[0][ENC_ETXSTH], (uint8_t)(ENC_TXSTART >> 8));

    /* TX end pointer: TXSTART + 1 (control byte) + 64 (frame) - 1 */
    uint16_t txend = (uint16_t)(ENC_TXSTART + 1U + 64U - 1U);
    ASSERT_EQ(model.regs[0][ENC_ETXNDL], (uint8_t)(txend & 0xFF));
    ASSERT_EQ(model.regs[0][ENC_ETXNDH], (uint8_t)(txend >> 8));

    /* Verify frame data in model's captured buffer */
    ASSERT_EQ(model.tx_captured[0], 0x00);  /* control byte */
    ASSERT_EQ(model.tx_captured[1], 0xBB);  /* frame byte */
    ASSERT_EQ(model.tx_captured[64], 0xBB); /* last frame byte */
}

/* --- 9. Read-only register protection (EREVID) --- */

TEST(test_readonly_erevid)
{
    setup_and_init();

    /* EREVID = 0x06 after init */
    ASSERT_EQ(model.regs[3][ENC_EREVID], 0x06);

    /* Try to overwrite EREVID via WCR (bank 3, address 0x12).
     * Need to be in bank 3. */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFC | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_BSEL0 | ENC_ECON1_BSEL1);
    model_spi.cs_high(model_spi.ctx);

    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFS | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_BSEL0 | ENC_ECON1_BSEL1); /* bank 3 */
    model_spi.cs_high(model_spi.ctx);

    /* Attempt WCR to EREVID */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_WCR | ENC_EREVID);
    model_spi.xfer(model_spi.ctx, 0xFF);
    model_spi.cs_high(model_spi.ctx);

    /* EREVID should still be 0x06 (read-only) */
    ASSERT_EQ(model.regs[3][ENC_EREVID], 0x06);

    /* Verify via driver API too */
    ASSERT_EQ(enc28j60_get_revision(&dev), 0x06);
}

/* --- 10. Receive packet via model-injected RX buffer --- */

TEST(test_receive_via_model)
{
    setup_and_init();

    /* Inject a packet at RXSTART */
    uint8_t frame[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    enc28j60_model_inject_rx(&model, ENC_RXSTART, ENC_RXSTART, frame, 8);

    /* Set packet count */
    enc28j60_model_set_pktcnt(&model, 1);

    /* Set ERDPT to RXSTART (the driver sets this during receive) */
    dev.next_packet_ptr = ENC_RXSTART;

    uint8_t out[64];
    uint16_t out_len = 0;
    enc28j60_err_t err = enc28j60_receive(&dev, out, sizeof(out), &out_len);
    ASSERT_EQ(err, ENC28J60_OK);
    ASSERT_EQ(out_len, 8);
    ASSERT_EQ(out[0], 0xDE);
    ASSERT_EQ(out[1], 0xAD);
    ASSERT_EQ(out[7], 0xBE);
}

/* --- 11. ESTAT is read-only --- */

TEST(test_estat_readonly)
{
    setup_and_init();

    /* ESTAT should have CLKRDY set: init polled until it was */
    ASSERT_TRUE(model.common[ENC_ESTAT] & ENC_ESTAT_CLKRDY);

    /* Try to clear ESTAT via WCR */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_WCR | ENC_ESTAT);
    model_spi.xfer(model_spi.ctx, 0x00);
    model_spi.cs_high(model_spi.ctx);

    /* Should still have CLKRDY */
    ASSERT_TRUE(model.common[ENC_ESTAT] & ENC_ESTAT_CLKRDY);
}

/* --- 12. Common registers shared across banks --- */

TEST(test_common_regs_across_banks)
{
    setup_and_init();

    /* Write to ECON2 (common, 0x1E) while in bank 0.
     * Then switch to bank 2 and verify the same value is visible. */

    /* First clear bank bits to ensure bank 0 */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFC | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_BSEL0 | ENC_ECON1_BSEL1);
    model_spi.cs_high(model_spi.ctx);

    /* Write ECON2 = 0x42 */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_WCR | ENC_ECON2);
    model_spi.xfer(model_spi.ctx, 0x42);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_EQ(model.common[ENC_ECON2], 0x42);

    /* Switch to bank 2 */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFS | ENC_ECON1);
    model_spi.xfer(model_spi.ctx, ENC_ECON1_BSEL1);
    model_spi.cs_high(model_spi.ctx);

    /* Read ECON2: should still be 0x42 */
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_RCR | ENC_ECON2);
    uint8_t val = model_spi.xfer(model_spi.ctx, 0xFF);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_EQ(val, 0x42);
}

/* --- 13. Oscillator Start-up Timer and the wait after SRC --- */

/* DS39662C p.7: "After a Power-on Reset ... the CLKRDY bit must be polled
 * before transmitting packets, enabling packet reception or accessing any
 * MAC, MII or PHY registers." */
TEST(test_init_polls_clkrdy_before_mac_access)
{
    setup();
    model.clkrdy_after = 3;
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_OK);
    ASSERT_EQ(model.estat_read_count, 3);
    ASSERT_EQ(model.early_access_count, 0);
    ASSERT_TRUE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);
}

TEST(test_init_times_out_when_clkrdy_never_sets)
{
    setup();
    model.clkrdy_after = 0;   /* no clock: the OST never expires */
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_ERR_TIMEOUT);
    ASSERT_EQ(model.early_access_count, 0);
    ASSERT_FALSE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);
}

/* Errata DS80349C issue 2: an SRC leaves CLKRDY set, so polling proves
 * nothing and init has to wait at least 1 ms. */
TEST(test_init_waits_after_src_when_clkrdy_already_set)
{
    setup();
    model.common[ENC_ESTAT] |= ENC_ESTAT_CLKRDY;   /* warm restart */
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_OK);
    ASSERT_TRUE(model.ms_since_src >= 1);
    ASSERT_EQ(model.early_access_count, 0);
}

/* A HAL that cannot wait cannot meet the errata's 1 ms */
TEST(test_init_refuses_hal_without_delay)
{
    enc28j60_spi_t no_delay = model_spi;
    no_delay.delay_ms = NULL;
    setup();
    ASSERT_EQ(enc28j60_init(&dev, &no_delay, test_mac), ENC28J60_ERR_PARAM);
    ASSERT_EQ(model.reset_count, 0);
}

/* --- 14. Duplex: PHCON1.PDPXMD has to match MACON3.FULDPX --- */

/* DS39662C p.55: "If only one of these two bits is set, the ENC28J60 will be
 * in an indeterminate state and not function correctly." LEDB sets PDPXMD at
 * Reset, and p.10 leaves it indeterminate with no LED attached. */
TEST(test_init_clears_pdpxmd_strapped_by_ledb)
{
    setup();
    enc28j60_model_set_phy(&model, ENC_PHCON1, ENC_PHCON1_PDPXMD);
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_OK);

    ASSERT_EQ(model.regs[2][ENC_MACON3] & 0x01U, 0);        /* FULDPX */
    ASSERT_EQ(model.phy_regs[ENC_PHCON1] & ENC_PHCON1_PDPXMD, 0);
    /* The write takes the whole register: the rest stays at its reset 0 */
    ASSERT_EQ(model.phy_regs[ENC_PHCON1], 0x0000);
    /* Polled to the end, and before RXEN as p.45 asks */
    ASSERT_FALSE(model.regs[3][ENC_MISTAT] & ENC_MISTAT_BUSY);
    ASSERT_EQ(model.phy_write_rx_on, 0);
    ASSERT_EQ(model.early_access_count, 0);
}

TEST(test_init_times_out_when_phy_stays_busy)
{
    setup();
    model.phy_busy_after = 0;   /* BUSY never clears */
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_ERR_TIMEOUT);
    ASSERT_FALSE(model.common[ENC_ECON1] & ENC_ECON1_RXEN);
}

/* DS39662C p.29: "a dummy byte will first be shifted out" for MAC and MII
 * registers, and ETH registers in the same bank have none */
TEST(test_model_dummy_byte_only_on_mac_mii)
{
    setup_and_init();

    /* Bank 3, where MISTAT is MII and EREVID is ETH */
    ASSERT_EQ(enc28j60_get_revision(&dev), 0x06);
    model.regs[3][ENC_MISTAT] = 0x04U;

    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_RCR | ENC_MISTAT);
    uint8_t dummy = model_spi.xfer(model_spi.ctx, 0xFF);
    uint8_t val = model_spi.xfer(model_spi.ctx, 0xFF);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_EQ(dummy, 0xFF);
    ASSERT_EQ(val, 0x04);
}

/* --- 15. Half-duplex loopback --- */

/* DS39662C p.55: "the Reset default configuration will loop transmitted
 * packets back to itself. Unless the receive filter configuration filters
 * these packets out, they will be written into the circular receive buffer".
 * The filters accept broadcast, so "the host controller should set the
 * PHCON2.HDLDIS bit." It is bit 8 of PHCON2 at PHY address 10h (p.39). */
TEST(test_init_disables_half_duplex_loopback)
{
    setup();
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_OK);

    ASSERT_EQ(model.regs[1][ENC_ERXFCON] & 0x01U, 0x01U);   /* BCEN is on */
    ASSERT_EQ(model.phy_regs[ENC_PHCON2], 0x0100);
    /* Two PHY writes now: the second must wait for the first */
    ASSERT_EQ(model.phy_write_while_busy, 0);
    ASSERT_EQ(model.phy_write_rx_on, 0);
}

/* --- 16. MACON4.DEFER --- */

/* DS39662C p.36: "Configure the bits in MACON4. For conformance to the IEEE
 * 802.3 standard, set the DEFER bit." It is bit 6 (p.38), and the others
 * stay at their reset 0. */
TEST(test_init_sets_macon4_defer)
{
    setup();
    ASSERT_EQ(enc28j60_init(&dev, &model_spi, test_mac), ENC28J60_OK);
    ASSERT_EQ(model.regs[2][ENC_MACON4], 0x40);
}

/* --- 17. Transmit abort --- */

/* DS39662C p.42: TXRTS clears when "the packet is finished transmitting or is
 * aborted", and "To check if the packet was successfully transmitted, the
 * ESTAT.TXABRT bit should be read." */
TEST(test_send_reports_tx_abort)
{
    setup_and_init();
    uint8_t frame[64];
    memset(frame, 0x5A, sizeof(frame));

    model.tx_end_estat = ENC_ESTAT_TXABRT;
    ASSERT_EQ(enc28j60_send(&dev, frame, 64), ENC28J60_ERR_TX_ABORT);
}

/* p.70: "the host controller should clear the LATECOL (if set) and TXABRT bits
 * so that future aborts can be detected accurately." */
TEST(test_send_after_abort_succeeds)
{
    setup_and_init();
    uint8_t frame[64];
    memset(frame, 0x5A, sizeof(frame));

    model.tx_end_estat = ENC_ESTAT_TXABRT | ENC_ESTAT_LATECOL;
    ASSERT_EQ(enc28j60_send(&dev, frame, 64), ENC28J60_ERR_TX_ABORT);

    ASSERT_EQ(enc28j60_send(&dev, frame, 64), ENC28J60_OK);
    ASSERT_FALSE(model.common[ENC_ESTAT] &
                 (ENC_ESTAT_TXABRT | ENC_ESTAT_LATECOL));
}

/* A flag left from before this frame says nothing about this frame */
TEST(test_send_ignores_stale_tx_abort)
{
    setup_and_init();
    uint8_t frame[64];
    memset(frame, 0x5A, sizeof(frame));

    model.common[ENC_ESTAT] |= ENC_ESTAT_TXABRT;
    ASSERT_EQ(enc28j60_send(&dev, frame, 64), ENC28J60_OK);
}

/* Register 12-1: BFC clears the R/C bits and leaves the R bits alone */
TEST(test_model_bfc_clears_only_rc_bits_of_estat)
{
    setup_and_init();
    model.common[ENC_ESTAT] |= ENC_ESTAT_TXABRT;

    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, ENC_OP_BFC | ENC_ESTAT);
    model_spi.xfer(model_spi.ctx, 0xFF);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_FALSE(model.common[ENC_ESTAT] & ENC_ESTAT_TXABRT);
    ASSERT_TRUE(model.common[ENC_ESTAT] & ENC_ESTAT_CLKRDY);
}

/* --- 18. Send timeout --- */

/* DS39662C p.42: while transmitting, "none of the bytes to be transmitted
 * should be read or written to through the SPI. If the host controller wishes
 * to cancel the transmission, it can clear the TXRTS bit." */
TEST(test_send_timeout_cancels_the_transmission)
{
    setup_and_init();
    uint8_t frame[64];
    memset(frame, 0x5A, sizeof(frame));

    model.tx_complete_after = 0;   /* TXRTS never clears by itself */
    ASSERT_EQ(enc28j60_send(&dev, frame, 64), ENC28J60_ERR_TIMEOUT);
    ASSERT_FALSE(model.common[ENC_ECON1] & ENC_ECON1_TXRTS);

    /* The next frame finds nothing pending */
    model.tx_complete_after = 1;
    ASSERT_EQ(enc28j60_send(&dev, frame, 64), ENC28J60_OK);
    ASSERT_EQ(model.tx_write_while_pending, 0);
}

TEST_MAIN("enc28j60 hardware model")
    RUN(test_init_sequence);
    RUN(test_bank_selection);
    RUN(test_register_rw_per_bank);
    RUN(test_buffer_write_read);
    RUN(test_phy_indirect_read);
    RUN(test_soft_reset_restores_defaults);
    RUN(test_bit_field_ops);
    RUN(test_tx_flow);
    RUN(test_readonly_erevid);
    RUN(test_receive_via_model);
    RUN(test_estat_readonly);
    RUN(test_common_regs_across_banks);
    RUN(test_init_polls_clkrdy_before_mac_access);
    RUN(test_init_times_out_when_clkrdy_never_sets);
    RUN(test_init_waits_after_src_when_clkrdy_already_set);
    RUN(test_init_refuses_hal_without_delay);
    RUN(test_init_clears_pdpxmd_strapped_by_ledb);
    RUN(test_init_times_out_when_phy_stays_busy);
    RUN(test_model_dummy_byte_only_on_mac_mii);
    RUN(test_init_disables_half_duplex_loopback);
    RUN(test_init_sets_macon4_defer);
    RUN(test_send_reports_tx_abort);
    RUN(test_send_after_abort_succeeds);
    RUN(test_send_ignores_stale_tx_abort);
    RUN(test_model_bfc_clears_only_rc_bits_of_estat);
    RUN(test_send_timeout_cancels_the_transmission);
TEST_MAIN_END()
