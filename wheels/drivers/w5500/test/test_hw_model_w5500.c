// SPDX-License-Identifier: MIT
/*
 * test_hw_model_w5500.c -- tests using the W5500 hardware register model
 *
 * Runs the driver against hw_model_w5500.h. Expected values come from the
 * W5500 Datasheet Version 1.1.0, and page numbers are those of that PDF.
 *
 * Tests: reset values, read-only and RCW1 registers, the lag between Sn_CR and
 * Sn_SR, OPEN, SEND and RECV pointer handling, buffer offsets past 2 KB and
 * past 0xFFFF.
 */

#include "test_harness.h"
#include "hw_model_w5500.h"
#include "../w5500.h"

static w5500_model_t model;

static const w5500_spi_t model_spi = {
    .cs_low   = w5500_model_cs_low,
    .cs_high  = w5500_model_cs_high,
    .xfer     = w5500_model_xfer,
    .delay_ms = w5500_model_delay_ms,
    .ctx      = &model,
};

static w5500_t dev;

static void setup(void)
{
    w5500_model_init(&model);
    memset(&dev, 0, sizeof(dev));
}

static void setup_and_init(void)
{
    setup();
    (void)w5500_init(&dev, &model_spi);
}

/* A socket open in UDP mode, with no peer needed to move data in the model */
static void setup_udp(uint8_t sock)
{
    setup_and_init();
    (void)w5500_socket_open(&dev, sock, W5500_PROTO_UDP, 5000);
}

/* --- raw SPI frames, for what the driver does not do by itself --- */

static uint8_t bsb_ctrl(uint8_t bsb, bool write)
{
    return (uint8_t)((uint8_t)(bsb << 3) | (write ? W5_CTRL_WRITE : W5_CTRL_READ));
}

static void frame_write(uint8_t bsb, uint16_t addr, uint8_t val)
{
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, (uint8_t)(addr >> 8));
    model_spi.xfer(model_spi.ctx, (uint8_t)(addr & 0xFFU));
    model_spi.xfer(model_spi.ctx, bsb_ctrl(bsb, true));
    model_spi.xfer(model_spi.ctx, val);
    model_spi.cs_high(model_spi.ctx);
}

static uint8_t frame_read(uint8_t bsb, uint16_t addr)
{
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, (uint8_t)(addr >> 8));
    model_spi.xfer(model_spi.ctx, (uint8_t)(addr & 0xFFU));
    model_spi.xfer(model_spi.ctx, bsb_ctrl(bsb, false));
    uint8_t val = model_spi.xfer(model_spi.ctx, 0x00U);
    model_spi.cs_high(model_spi.ctx);
    return val;
}

static uint16_t reg16(uint8_t sock, uint16_t addr)
{
    return (uint16_t)(((uint16_t)model.sock_regs[sock][addr] << 8) |
                      model.sock_regs[sock][addr + 1U]);
}

static void set_reg16(uint8_t sock, uint16_t addr, uint16_t val)
{
    model.sock_regs[sock][addr]      = (uint8_t)(val >> 8);
    model.sock_regs[sock][addr + 1U] = (uint8_t)(val & 0xFFU);
}

/* --- 1. Reset values --- */

/* Register notation [R/W] [address] [reset value], pages 32 to 43 */
static const struct { uint16_t addr; uint8_t val; } common_reset[] = {
    {0x0000, 0x00},   /* MR */
    {0x0001, 0x00},   /* GAR */
    {0x0009, 0x00},   /* SHAR */
    {0x000F, 0x00},   /* SIPR */
    {0x0019, 0x07},   /* RTR, 0x07D0 */
    {0x001A, 0xD0},
    {0x001B, 0x08},   /* RCR */
    {0x001C, 0x28},   /* PTIMER */
    {0x0026, 0xFF},   /* PMRU, 0xFFFF */
    {0x0027, 0xFF},
    {0x0039, 0x04},   /* VERSIONR */
};

/* Pages 44 to 57 */
static const struct { uint16_t addr; uint8_t val; } sock_reset[] = {
    {0x0000, 0x00},   /* Sn_MR */
    {0x0001, 0x00},   /* Sn_CR */
    {0x0002, 0x00},   /* Sn_IR */
    {0x0003, 0x00},   /* Sn_SR */
    {0x0006, 0xFF},   /* Sn_DHAR, 0xFFFFFFFFFFFF */
    {0x000B, 0xFF},
    {0x0016, 0x80},   /* Sn_TTL */
    {0x001E, 0x02},   /* Sn_RXBUF_SIZE */
    {0x001F, 0x02},   /* Sn_TXBUF_SIZE */
    {0x0020, 0x08},   /* Sn_TX_FSR, 0x0800 */
    {0x0021, 0x00},
    {0x0026, 0x00},   /* Sn_RX_RSR */
    {0x002C, 0xFF},   /* Sn_IMR */
    {0x002D, 0x40},   /* Sn_FRAG, 0x4000 */
    {0x002E, 0x00},
};

static void assert_reset_values(void)
{
    for (size_t i = 0; i < sizeof(common_reset) / sizeof(common_reset[0]); i++) {
        ASSERT_EQ_HEX(model.common_regs[common_reset[i].addr],
                      common_reset[i].val);
    }
    for (uint8_t s = 0; s < W5_MAX_SOCKETS; s++) {
        for (size_t i = 0; i < sizeof(sock_reset) / sizeof(sock_reset[0]); i++) {
            ASSERT_EQ_HEX(model.sock_regs[s][sock_reset[i].addr],
                          sock_reset[i].val);
        }
    }
    /* p.42: PHYCFGR [0b10111XXX], the low 3 bits being link status */
    ASSERT_EQ_HEX(model.common_regs[0x002E] & 0xF8U, 0xB8);
}

TEST(test_model_power_on_reset_values)
{
    setup();
    assert_reset_values();
}

/* p.32: "If this bit is '1', All internal registers will be initialized. It
 * will be automatically cleared as '0' after S/W reset." */
TEST(test_model_mr_rst_restores_reset_values)
{
    setup();
    frame_write(0, 0x0019, 0x11);                  /* RTR */
    frame_write(0, 0x001B, 0x22);                  /* RCR */
    frame_write(W5_BSB_SOCK_REG(3), 0x0016, 0x33); /* Sn_TTL */
    frame_write(W5_BSB_SOCK_REG(3), 0x002C, 0x00); /* Sn_IMR */
    ASSERT_EQ(model.common_regs[0x0019], 0x11);
    ASSERT_EQ(model.sock_regs[3][0x002C], 0x00);

    frame_write(0, 0x0000, 0x80);

    assert_reset_values();
}

TEST(test_init_finds_the_chip_and_keeps_2kb_buffers)
{
    setup();
    ASSERT_EQ(w5500_init(&dev, &model_spi), W5500_OK);
    ASSERT_EQ(model.reset_count, 1);
    assert_reset_values();

    /* p.31: "the total sum of Sn_TXBUF_SIZE should be not exceed 16" */
    unsigned tx = 0, rx = 0;
    for (uint8_t s = 0; s < W5_MAX_SOCKETS; s++) {
        tx += model.sock_regs[s][0x001F];
        rx += model.sock_regs[s][0x001E];
    }
    ASSERT_EQ(tx, 16);
    ASSERT_EQ(rx, 16);
}

/* --- 2. Read-only and RCW1 registers --- */

/* p.49 Sn_SR [R], p.53 Sn_TX_FSR [R], p.54 Sn_TX_RD [R], p.55 Sn_RX_RSR [R] and
 * Sn_RX_WR [R] */
TEST(test_model_writes_leave_read_only_socket_registers)
{
    static const uint16_t ro[] = {0x0003, 0x0020, 0x0021, 0x0022, 0x0023,
                                  0x0026, 0x0027, 0x002A, 0x002B};
    setup_udp(1);

    for (size_t i = 0; i < sizeof(ro) / sizeof(ro[0]); i++) {
        uint8_t before = model.sock_regs[1][ro[i]];
        frame_write(W5_BSB_SOCK_REG(1), ro[i], (uint8_t)~before);
        ASSERT_EQ_HEX(model.sock_regs[1][ro[i]], before);
    }
}

/* p.43 VERSIONR [R], p.41 UIPR and UPORTR [R] */
TEST(test_model_writes_leave_read_only_common_registers)
{
    setup();
    frame_write(0, 0x0039, 0x55);
    ASSERT_EQ(model.common_regs[0x0039], 0x04);
    frame_write(0, 0x0028, 0x55);
    ASSERT_EQ(model.common_regs[0x0028], 0x00);
    frame_write(0, 0x002D, 0x55);
    ASSERT_EQ(model.common_regs[0x002D], 0x00);
}

/* p.32 note 2: "ReadClearWrite1 (RCW1) Software can read as well as clear this
 * bit by writing 1. Writing '0' has no effect on the bit value." */
TEST(test_model_sn_ir_is_read_clear_write_1)
{
    setup_and_init();
    model.sock_regs[2][W5_SN_IR] = 0x1FU;

    frame_write(W5_BSB_SOCK_REG(2), W5_SN_IR, 0x00U);
    ASSERT_EQ_HEX(model.sock_regs[2][W5_SN_IR], 0x1F);

    frame_write(W5_BSB_SOCK_REG(2), W5_SN_IR, 0x10U);
    ASSERT_EQ_HEX(model.sock_regs[2][W5_SN_IR], 0x0F);
}

/* --- 3. Sn_CR clears before the command has finished --- */

/* p.46: "Even though Sn_CR is cleared to 0x00, the command is still being
 * processed. To check whether the command is completed or not, please check
 * the Sn_IR or Sn_SR." */
TEST(test_model_sn_sr_moves_after_sn_cr_clears)
{
    setup_udp(0);
    model.cr_lag = 0;
    model.sr_lag = 2;

    frame_write(W5_BSB_SOCK_REG(0), W5_SN_CR, W5_CMD_CLOSE);

    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_CR), 0x00);
    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_SR), W5_SOCK_UDP);
    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_SR), W5_SOCK_UDP);
    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_SR), W5_SOCK_CLOSED);
}

/* p.46: "After W5500 accepts the command, the Sn_CR register is automatically
 * cleared to 0x00." */
TEST(test_model_sn_cr_holds_the_command_until_accepted)
{
    setup_udp(0);
    model.cr_lag = 2;

    frame_write(W5_BSB_SOCK_REG(0), W5_SN_CR, W5_CMD_CLOSE);

    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_CR), W5_CMD_CLOSE);
    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_CR), W5_CMD_CLOSE);
    ASSERT_EQ(frame_read(W5_BSB_SOCK_REG(0), W5_SN_CR), 0x00);
}

TEST(test_open_waits_out_the_status_lag)
{
    static const struct { uint8_t proto, status; } cases[] = {
        {W5500_PROTO_TCP,    0x13},   /* p.46: SOCK_INIT */
        {W5500_PROTO_UDP,    0x22},   /* SOCK_UDP */
        {W5500_PROTO_MACRAW, 0x42},   /* SOCK_MACRAW */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        setup_and_init();
        model.sr_lag = 5;
        ASSERT_EQ(w5500_socket_open(&dev, 0, cases[i].proto, 80), W5500_OK);
        ASSERT_EQ(model.sock_regs[0][W5_SN_SR], cases[i].status);
        ASSERT_EQ(model.open_not_closed, 0);
    }
}

/* Reopening: OPEN must not reach a socket whose CLOSE is still going */
TEST(test_reopen_waits_for_the_close)
{
    setup_udp(0);
    model.sr_lag = 5;

    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 80), W5500_OK);

    ASSERT_EQ(model.open_not_closed, 0);
    ASSERT_EQ(model.sock_regs[0][W5_SN_SR], W5_SOCK_INIT);
}

TEST(test_close_returns_with_the_socket_closed)
{
    setup_udp(0);
    model.sr_lag = 5;

    ASSERT_EQ(w5500_socket_close(&dev, 0), W5500_OK);

    ASSERT_EQ(model.sock_regs[0][W5_SN_SR], W5_SOCK_CLOSED);
}

/* p.51: "Socket 0 Port = 5000(0x1388)" goes in as 0x13 then 0x88, and "should
 * be set before OPEN command is ordered" */
TEST(test_open_programs_mode_and_port)
{
    setup_and_init();
    ASSERT_EQ(w5500_socket_open(&dev, 5, W5500_PROTO_UDP, 0x1234), W5500_OK);

    ASSERT_EQ(model.sock_regs[5][W5_SN_MR] & 0x0FU, 0x02);
    ASSERT_EQ(model.sock_regs[5][W5_SN_PORT0], 0x12);
    ASSERT_EQ(model.sock_regs[5][W5_SN_PORT1], 0x34);
}

/* p.45: "MACRAW mode should be only used in Socket 0." */
TEST(test_macraw_opens_on_socket_0_only)
{
    setup_and_init();
    ASSERT_EQ(w5500_socket_open(&dev, 1, W5500_PROTO_MACRAW, 0), W5500_ERR_SOCK);
    ASSERT_EQ(model.sock_regs[1][W5_SN_SR], W5_SOCK_CLOSED);
    ASSERT_EQ(model.macraw_not_sock0, 0);
    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_MACRAW, 0), W5500_OK);
    ASSERT_EQ(model.sock_regs[0][W5_SN_SR], W5_SOCK_MACRAW);
}

/* --- 4. OPEN initialises the pointers --- */

/* p.53: Sn_TX_FSR "is initialized to the configured size by Sn_TXBUF_SIZE".
 * p.54 and p.55: Sn_TX_RD, Sn_TX_WR, Sn_RX_RD "is initialized by OPEN command". */
TEST(test_open_initialises_the_pointers)
{
    setup_udp(0);
    uint8_t data[64] = {0};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 64), W5500_OK);
    w5500_model_inject_rx(&model, 0, data, 32);
    ASSERT_EQ(reg16(0, W5_SN_TX_RD0), 64);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 32);

    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_UDP, 5000), W5500_OK);

    ASSERT_EQ(reg16(0, W5_SN_TX_RD0), 0);
    ASSERT_EQ(reg16(0, W5_SN_TX_WR0), 0);
    ASSERT_EQ(reg16(0, W5_SN_RX_RD0), 0);
    ASSERT_EQ(reg16(0, W5_SN_RX_WR0), 0);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 0);
    ASSERT_EQ(reg16(0, W5_SN_TX_FSR0), 0x0800);
}

/* --- 5. SEND --- */

/* p.54: "After transmitting the saved data, the SEND command increases the
 * Sn_TX_RD as same as the Sn_TX_WR." */
TEST(test_send_moves_tx_rd_up_to_tx_wr)
{
    setup_udp(0);
    uint8_t data[100];
    memset(data, 0xA5, sizeof(data));

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 100), W5500_OK);

    ASSERT_EQ(reg16(0, W5_SN_TX_WR0), 100);
    ASSERT_EQ(reg16(0, W5_SN_TX_RD0), 100);
    ASSERT_EQ(reg16(0, W5_SN_TX_FSR0), 0x0800);
    ASSERT_EQ(model.sent_len[0], 100);
    ASSERT_MEMEQ_N(model.sent[0], data, 100);
    ASSERT_EQ(model.empty_frames, 0);
}

/* p.48: "In order to clear the Sn_IR bit, the host should write the bit to '1'" */
TEST(test_send_clears_send_ok)
{
    setup_udp(0);
    uint8_t data[4] = {1, 2, 3, 4};

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_OK);

    ASSERT_FALSE(model.sock_regs[0][W5_SN_IR] & W5_IR_SEND_OK);
}

/* p.48: SEND_OK "is issued when SEND command is completed", after Sn_CR clears */
TEST(test_send_waits_for_send_ok)
{
    setup_udp(0);
    uint8_t data[4] = {1, 2, 3, 4};
    model.ir_lag = 7;

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_OK);

    ASSERT_EQ(reg16(0, W5_SN_TX_RD0), 4);    /* it really had finished */
    ASSERT_EQ(model.send_while_busy, 0);
}

/* p.48: TIMEOUT "is issued when ARPTO or TCPTO occurs", and p.49: "when a
 * timeout occurs, it is changed to SOCK_CLOSED regardless of previous status" */
TEST(test_send_reports_the_chip_timeout)
{
    setup_udp(0);
    uint8_t data[4] = {1, 2, 3, 4};
    model.send_result = W5_IR_TIMEOUT;

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);

    ASSERT_FALSE(model.sock_regs[0][W5_SN_IR] & W5_IR_TIMEOUT);
    ASSERT_EQ(model.sock_regs[0][W5_SN_SR], W5_SOCK_CLOSED);
}

/* A SEND_OK left over from an earlier frame says nothing about this one */
TEST(test_send_ignores_a_stale_send_ok)
{
    setup_udp(0);
    uint8_t data[4] = {1, 2, 3, 4};
    model.sock_regs[0][W5_SN_IR] = W5_IR_SEND_OK;
    model.send_result = 0;   /* this SEND never ends */

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);
}

/* p.48: "When an interrupt occurs and the corresponding bit of Sn_IMR is '1',
 * the corresponding bit of Sn_IR becomes '1'." Sn_IMR resets to 0xFF, which is
 * what lets the driver poll SEND_OK. */
TEST(test_model_sn_imr_gates_sn_ir)
{
    setup_udp(0);
    uint8_t data[4] = {1, 2, 3, 4};
    model.sock_regs[0][0x002C] = (uint8_t)~W5_IR_SEND_OK;

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);
}

/* p.53: "Data bigger than Sn_TX_FSR should not be saved in the Socket n TX
 * Buffer because the bigger data overwrites the previous saved data not yet
 * sent." Here 100 bytes are queued and not yet sent. */
TEST(test_send_refuses_more_than_the_free_size)
{
    setup_udp(0);
    static uint8_t data[2048];
    set_reg16(0, W5_SN_TX_WR0, 100);

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 1949), W5500_ERR_SIZE);
    ASSERT_EQ(reg16(0, W5_SN_TX_WR0), 100);

    /* p.53: "if data is equal or smaller than its checked size, transmit" */
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 1948), W5500_OK);
    ASSERT_EQ(model.tx_overruns, 0);
}

/* --- 6. Offsets past the 2 KB of the buffer --- */

/* p.27: "Regardless of the allocated size of each Socket TX/RX Buffer, it can
 * be accessible within the 16 bits offset address range (From 0x0000 to
 * 0xFFFF)." */
TEST(test_model_buffer_offsets_alias_past_2kb)
{
    setup_and_init();

    frame_write(W5_BSB_SOCK_TX(2), 0x0805, 0x5A);
    ASSERT_EQ(model.tx_buf[2][0x0005], 0x5A);
    ASSERT_EQ(frame_read(W5_BSB_SOCK_TX(2), 0xF805), 0x5A);

    model.rx_buf[2][0x07FF] = 0xA5;
    ASSERT_EQ(frame_read(W5_BSB_SOCK_RX(2), 0xFFFF), 0xA5);
}

/* The second use of a pointer past 2 KB is what the old model lost */
TEST(test_send_keeps_working_past_2kb)
{
    setup_udp(0);
    static uint8_t data[1000];

    for (unsigned round = 0; round < 5U; round++) {
        memset(data, (int)(0x30U + round), sizeof(data));
        ASSERT_EQ(w5500_socket_send(&dev, 0, data, 1000), W5500_OK);
        ASSERT_EQ(model.sent_len[0], 1000);
        ASSERT_MEMEQ_N(model.sent[0], data, 1000);
    }
    ASSERT_EQ(reg16(0, W5_SN_TX_RD0), 5000);
}

/* p.55: "If the increment value exceeds the maximum value 0xFFFF ... then the
 * carry bit is ignored and will automatically update with the lower 16bits
 * value." */
TEST(test_send_across_the_16_bit_wrap)
{
    setup_udp(0);
    uint8_t data[32];
    for (uint8_t i = 0; i < 32U; i++) data[i] = (uint8_t)(i + 1U);
    set_reg16(0, W5_SN_TX_RD0, 0xFFF0);
    set_reg16(0, W5_SN_TX_WR0, 0xFFF0);

    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 32), W5500_OK);

    ASSERT_EQ(reg16(0, W5_SN_TX_WR0), 0x0010);
    ASSERT_EQ(reg16(0, W5_SN_TX_RD0), 0x0010);
    ASSERT_EQ(reg16(0, W5_SN_TX_FSR0), 0x0800);
    ASSERT_MEMEQ_N(model.sent[0], data, 32);
}

/* --- 7. RECV --- */

/* p.55: read, update Sn_RX_RD, then "Order RECV command is for notifying the
 * updated Sn_RX_RD to W5500." Sn_RX_RSR is the difference of the pointers. */
TEST(test_recv_frees_what_it_read)
{
    setup_udp(0);
    uint8_t in[30];
    uint8_t out[30];
    uint16_t got = 0;
    for (uint8_t i = 0; i < 30U; i++) in[i] = (uint8_t)(0x40U + i);

    w5500_model_inject_rx(&model, 0, in, 30);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 30);
    ASSERT_EQ(reg16(0, W5_SN_RX_WR0), 30);

    ASSERT_EQ(w5500_socket_recv(&dev, 0, out, 30, &got), W5500_OK);

    ASSERT_EQ(got, 30);
    ASSERT_MEMEQ_N(out, in, 30);
    ASSERT_EQ(reg16(0, W5_SN_RX_RD0), 30);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 0);
    ASSERT_EQ(w5500_socket_recv(&dev, 0, out, 30, &got), W5500_ERR_NO_DATA);
}

TEST(test_recv_takes_what_fits_and_leaves_the_rest)
{
    setup_udp(0);
    uint8_t in[30];
    uint8_t out[30];
    uint16_t got = 0;
    for (uint8_t i = 0; i < 30U; i++) in[i] = (uint8_t)(0x40U + i);
    w5500_model_inject_rx(&model, 0, in, 30);

    ASSERT_EQ(w5500_socket_recv(&dev, 0, out, 10, &got), W5500_OK);
    ASSERT_EQ(got, 10);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 20);

    ASSERT_EQ(w5500_socket_recv(&dev, 0, out, 30, &got), W5500_OK);
    ASSERT_EQ(got, 20);
    ASSERT_MEMEQ_N(out, in + 10, 20);
}

/* p.55: a new Sn_RX_RD counts only once RECV has told the chip */
TEST(test_model_rx_rd_counts_only_after_recv)
{
    setup_udp(0);
    uint8_t in[8] = {0};
    w5500_model_inject_rx(&model, 0, in, 8);

    frame_write(W5_BSB_SOCK_REG(0), W5_SN_RX_RD0, 0x00);
    frame_write(W5_BSB_SOCK_REG(0), W5_SN_RX_RD1, 0x08);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 8);

    model.cr_lag = 0;
    frame_write(W5_BSB_SOCK_REG(0), W5_SN_CR, W5_CMD_RECV);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 0);
}

/* p.48: RECV "is issued whenever data is received from a peer" */
TEST(test_model_received_data_sets_recv)
{
    setup_udp(0);
    uint8_t in[8] = {0};
    w5500_model_inject_rx(&model, 0, in, 8);
    ASSERT_TRUE(model.sock_regs[0][W5_SN_IR] & W5_IR_RECV);
}

TEST(test_recv_keeps_working_past_2kb)
{
    setup_udp(3);
    static uint8_t in[1000];
    static uint8_t out[1000];
    uint16_t got = 0;

    for (unsigned round = 0; round < 5U; round++) {
        memset(in, (int)(0x50U + round), sizeof(in));
        w5500_model_inject_rx(&model, 3, in, 1000);
        ASSERT_EQ(w5500_socket_recv(&dev, 3, out, 1000, &got), W5500_OK);
        ASSERT_EQ(got, 1000);
        ASSERT_MEMEQ_N(out, in, 1000);
    }
    ASSERT_EQ(reg16(3, W5_SN_RX_RD0), 5000);
}

/* p.55: "If the increment value exceeds the maximum value 0xFFFF, that is, is
 * greater than 0x10000 and the carry bit occurs, update with the lower 16bits
 * value ignored the carry bit." */
TEST(test_recv_across_the_16_bit_wrap)
{
    setup_udp(0);
    uint8_t in[32];
    uint8_t out[32];
    uint16_t got = 0;
    for (uint8_t i = 0; i < 32U; i++) in[i] = (uint8_t)(0x80U + i);
    w5500_model_set_rx_pointer(&model, 0, 0xFFF0);

    w5500_model_inject_rx(&model, 0, in, 32);
    ASSERT_EQ(w5500_socket_recv(&dev, 0, out, 32, &got), W5500_OK);

    ASSERT_EQ(got, 32);
    ASSERT_MEMEQ_N(out, in, 32);
    ASSERT_EQ(reg16(0, W5_SN_RX_RD0), 0x0010);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 0);
}

/* --- 8. SPI frames --- */

/* p.17: "Variable Data Length Mode, N-Bytes Data Phase (1 <= N)" */
TEST(test_model_counts_a_frame_with_no_data)
{
    setup();
    model_spi.cs_low(model_spi.ctx);
    model_spi.xfer(model_spi.ctx, 0x00);
    model_spi.xfer(model_spi.ctx, 0x39);
    model_spi.xfer(model_spi.ctx, 0x00);
    model_spi.cs_high(model_spi.ctx);

    ASSERT_EQ(model.empty_frames, 1);
}

TEST(test_recv_with_no_room_sends_no_empty_frame)
{
    setup_udp(0);
    uint8_t in[8] = {0};
    uint8_t out[1];
    uint16_t got = 0;
    w5500_model_inject_rx(&model, 0, in, 8);

    ASSERT_EQ(w5500_socket_recv(&dev, 0, out, 0, &got), W5500_ERR_PARAM);

    ASSERT_EQ(model.empty_frames, 0);
    ASSERT_EQ(reg16(0, W5_SN_RX_RSR0), 8);
}

TEST(test_set_mac_and_ip_reach_their_registers)
{
    static const uint8_t mac[6]  = {0x00, 0x08, 0xDC, 0x01, 0x02, 0x03};
    static const uint8_t ip[4]   = {192, 168, 0, 10};
    static const uint8_t mask[4] = {255, 255, 255, 0};
    static const uint8_t gw[4]   = {192, 168, 0, 1};
    uint8_t read_back[6];

    setup_and_init();
    ASSERT_EQ(w5500_set_mac(&dev, mac), W5500_OK);
    ASSERT_EQ(w5500_set_ip(&dev, ip, mask, gw), W5500_OK);

    w5500_model_get_mac(&model, read_back);
    ASSERT_MEMEQ_N(read_back, mac, 6);
    ASSERT_MEMEQ_N(&model.common_regs[0x000F], ip, 4);    /* SIPR */
    ASSERT_MEMEQ_N(&model.common_regs[0x0005], mask, 4);  /* SUBR */
    ASSERT_MEMEQ_N(&model.common_regs[0x0001], gw, 4);    /* GAR */
}

TEST_MAIN("w5500 hardware model")
    RUN(test_model_power_on_reset_values);
    RUN(test_model_mr_rst_restores_reset_values);
    RUN(test_init_finds_the_chip_and_keeps_2kb_buffers);
    RUN(test_model_writes_leave_read_only_socket_registers);
    RUN(test_model_writes_leave_read_only_common_registers);
    RUN(test_model_sn_ir_is_read_clear_write_1);
    RUN(test_model_sn_sr_moves_after_sn_cr_clears);
    RUN(test_model_sn_cr_holds_the_command_until_accepted);
    RUN(test_open_waits_out_the_status_lag);
    RUN(test_reopen_waits_for_the_close);
    RUN(test_close_returns_with_the_socket_closed);
    RUN(test_open_programs_mode_and_port);
    RUN(test_macraw_opens_on_socket_0_only);
    RUN(test_open_initialises_the_pointers);
    RUN(test_send_moves_tx_rd_up_to_tx_wr);
    RUN(test_send_clears_send_ok);
    RUN(test_send_waits_for_send_ok);
    RUN(test_send_reports_the_chip_timeout);
    RUN(test_send_ignores_a_stale_send_ok);
    RUN(test_model_sn_imr_gates_sn_ir);
    RUN(test_send_refuses_more_than_the_free_size);
    RUN(test_model_buffer_offsets_alias_past_2kb);
    RUN(test_send_keeps_working_past_2kb);
    RUN(test_send_across_the_16_bit_wrap);
    RUN(test_recv_frees_what_it_read);
    RUN(test_recv_takes_what_fits_and_leaves_the_rest);
    RUN(test_model_rx_rd_counts_only_after_recv);
    RUN(test_model_received_data_sets_recv);
    RUN(test_recv_keeps_working_past_2kb);
    RUN(test_recv_across_the_16_bit_wrap);
    RUN(test_model_counts_a_frame_with_no_data);
    RUN(test_recv_with_no_room_sends_no_empty_frame);
    RUN(test_set_mac_and_ip_reach_their_registers);
TEST_MAIN_END()
