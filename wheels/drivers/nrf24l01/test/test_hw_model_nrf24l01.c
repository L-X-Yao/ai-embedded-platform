// SPDX-License-Identifier: MIT
/*
 * test_hw_model_nrf24l01.c -- tests using nRF24L01+ hardware register model
 *
 * Validates nRF24L01 driver against a register-accurate model with
 * SPI command protocol, FIFO simulation, and status flag behavior.
 * Tests init config, channel/address setup, mode switching, send/receive,
 * data rate/power configuration, and flush operations.
 */

#include "test_harness.h"
#include "hw_model_nrf24l01.h"
#include "../nrf24l01.h"

static nrf24_model_t model;

static const nrf24_spi_t model_spi = {
    .cs_low   = nrf24_model_cs_low,
    .cs_high  = nrf24_model_cs_high,
    .xfer     = nrf24_model_xfer,
    .ce_low   = nrf24_model_ce_low,
    .ce_high  = nrf24_model_ce_high,
    .delay_us = nrf24_model_delay_us,
    .ctx      = &model,
};

static void setup(void)
{
    nrf24_model_init(&model);
}

/* --- driving the model the way an MCU drives the chip --- */

static uint8_t spi_read(uint8_t reg)
{
    nrf24_model_cs_low(&model);
    nrf24_model_xfer(&model, (uint8_t)(NRF_CMD_R_REGISTER | reg));
    uint8_t val = nrf24_model_xfer(&model, NRF_CMD_NOP);
    nrf24_model_cs_high(&model);
    return val;
}

static void spi_read_multi(uint8_t reg, uint8_t *out, uint8_t len)
{
    nrf24_model_cs_low(&model);
    nrf24_model_xfer(&model, (uint8_t)(NRF_CMD_R_REGISTER | reg));
    for (uint8_t i = 0; i < len; i++) {
        out[i] = nrf24_model_xfer(&model, NRF_CMD_NOP);
    }
    nrf24_model_cs_high(&model);
}

static void spi_write(uint8_t reg, uint8_t val)
{
    nrf24_model_cs_low(&model);
    nrf24_model_xfer(&model, (uint8_t)(NRF_CMD_W_REGISTER | reg));
    nrf24_model_xfer(&model, val);
    nrf24_model_cs_high(&model);
}

static uint8_t spi_status(void)
{
    nrf24_model_cs_low(&model);
    uint8_t st = nrf24_model_xfer(&model, NRF_CMD_NOP);
    nrf24_model_cs_high(&model);
    return st;
}

static void spi_upload(uint8_t byte)
{
    nrf24_model_cs_low(&model);
    nrf24_model_xfer(&model, NRF_CMD_W_TX_PAYLOAD);
    nrf24_model_xfer(&model, byte);
    nrf24_model_cs_high(&model);
}

static void spi_read_payload(uint8_t *out, uint8_t len)
{
    nrf24_model_cs_low(&model);
    nrf24_model_xfer(&model, NRF_CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < len; i++) {
        out[i] = nrf24_model_xfer(&model, NRF_CMD_NOP);
    }
    nrf24_model_cs_high(&model);
}

static void ce_pulse(uint32_t us)
{
    nrf24_model_ce_high(&model);
    nrf24_model_delay_us(&model, us);
    nrf24_model_ce_low(&model);
}

/* Past TPOR and Tpd2stby, in standby-I (p22, p24) */
static void power_up(uint8_t prim_rx)
{
    nrf24_model_delay_us(&model, 100000);
    spi_write(NRF_REG_CONFIG, (uint8_t)(0x0A | prim_rx));
    nrf24_model_delay_us(&model, 1500);
}

static void listen_on_pipe1(uint8_t width)
{
    power_up(1);
    spi_write(NRF_REG_RX_PW_P0 + 1, width);
    nrf24_model_ce_high(&model);
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    nrf24_t dev;
    nrf24_err_t err = nrf24_init(&dev, &model_spi, 76, 8);
    ASSERT_EQ(err, NRF24_OK);
    ASSERT_EQ(dev.channel, 76);
    ASSERT_EQ(dev.payload_size, 8);
}

TEST(test_init_writes_config)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    uint8_t config = model.hw.regs[NRF_REG_CONFIG];
    /* PWR_UP (bit 1) should be set */
    ASSERT_TRUE(config & 0x02);
    /* EN_CRC (bit 3) should be set */
    ASSERT_TRUE(config & 0x08);
}

TEST(test_init_sets_channel)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 100, 16);
    ASSERT_EQ(model.hw.regs[NRF_REG_RF_CH], 100);
}

TEST(test_init_sets_payload_size)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 50, 20);
    ASSERT_EQ(model.hw.regs[NRF_REG_RX_PW_P0], 20);
}

TEST(test_init_rejects_bad_channel)
{
    setup();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 126, 8), NRF24_ERR_PARAM);
}

TEST(test_init_rejects_bad_payload)
{
    setup();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 0), NRF24_ERR_PARAM);
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 33), NRF24_ERR_PARAM);
}

/* --- set mode tests --- */

TEST(test_set_mode_rx)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    nrf24_set_mode(&dev, NRF24_MODE_RX);
    uint8_t config = model.hw.regs[NRF_REG_CONFIG];
    ASSERT_TRUE(config & 0x01);  /* PRIM_RX set */
    ASSERT_TRUE(model.ce_high);  /* CE high for RX */
}

TEST(test_set_mode_tx)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);
    nrf24_set_mode(&dev, NRF24_MODE_RX);

    nrf24_set_mode(&dev, NRF24_MODE_TX);
    uint8_t config = model.hw.regs[NRF_REG_CONFIG];
    ASSERT_FALSE(config & 0x01);  /* PRIM_RX cleared */
    ASSERT_FALSE(model.ce_high);  /* CE low for TX idle */
}

/* --- data rate and power tests --- */

TEST(test_set_data_rate_250kbps)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    nrf24_set_data_rate(&dev, NRF24_RATE_250KBPS);
    uint8_t rf = model.hw.regs[NRF_REG_RF_SETUP];
    ASSERT_TRUE(rf & 0x20);   /* RF_DR_LOW set */
    ASSERT_FALSE(rf & 0x08);  /* RF_DR_HIGH clear */
}

TEST(test_set_power_0dbm)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    nrf24_set_power(&dev, NRF24_POWER_0DBM);
    uint8_t rf = model.hw.regs[NRF_REG_RF_SETUP];
    ASSERT_EQ((rf >> 1) & 0x03, 3);  /* PWR bits = 11 */
}

/* --- send test --- */

TEST(test_send_success)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    nrf24_err_t err = nrf24_send(&dev, data, 8);
    ASSERT_EQ(err, NRF24_OK);
    ASSERT_TRUE(model.tx_count >= 1);
}

TEST(test_send_max_rt)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    model.force_max_rt = true;

    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    nrf24_err_t err = nrf24_send(&dev, data, 8);
    ASSERT_EQ(err, NRF24_ERR_MAX_RT);
}

/* --- receive test --- */

TEST(test_receive_data)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);
    nrf24_set_mode(&dev, NRF24_MODE_RX);

    /* Inject RX packet */
    uint8_t rx_data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    nrf24_model_inject_rx(&model, rx_data, 8);

    ASSERT_TRUE(nrf24_data_ready(&dev));

    uint8_t buf[8] = {0};
    uint8_t len = 0;
    nrf24_err_t err = nrf24_receive(&dev, buf, 8, &len);
    ASSERT_EQ(err, NRF24_OK);
    ASSERT_EQ(len, 8);
    ASSERT_EQ_HEX(buf[0], 0xAA);
    ASSERT_EQ_HEX(buf[7], 0x44);
}

/* --- flush tests --- */

TEST(test_flush_tx)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    spi_upload(0x55);
    spi_upload(0x66);
    ASSERT_EQ(spi_read(NRF_REG_FIFO_STATUS) & 0x10, 0);

    nrf24_err_t err = nrf24_flush_tx(&dev);
    ASSERT_EQ(err, NRF24_OK);
    ASSERT_TRUE(spi_read(NRF_REG_FIFO_STATUS) & 0x10);
}

TEST(test_read_status)
{
    setup();
    nrf24_t dev;
    nrf24_init(&dev, &model_spi, 76, 8);

    uint8_t st = nrf24_read_status(&dev);
    /* Should not have any flag bits set after init clear */
    ASSERT_FALSE(st & NRF_STATUS_RX_DR);
    ASSERT_FALSE(st & NRF_STATUS_TX_DS);
    ASSERT_FALSE(st & NRF_STATUS_MAX_RT);
}

TEST(test_model_address_writes_leave_rx_pw_alone)
{
    setup();
    nrf24_t dev;
    static const uint8_t addr[5] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 20), NRF24_OK);
    ASSERT_EQ(nrf24_set_tx_addr(&dev, addr, 5), NRF24_OK);
    ASSERT_EQ(model.hw.regs[NRF_REG_RX_PW_P0], 20);
    ASSERT_MEMEQ_N(model.addr_tx, addr, 5);
}

TEST(test_model_rx_fifo_pops_on_read)
{
    setup();
    static const uint8_t one[1] = { 0x11 };
    static const uint8_t two[1] = { 0x22 };
    power_up(1);
    spi_write(NRF_REG_RX_PW_P0, 1);
    nrf24_model_ce_high(&model);
    nrf24_model_inject_rx(&model, one, 1);
    nrf24_model_inject_rx(&model, two, 1);
    ASSERT_EQ(model.rx_count, 2);
    ASSERT_EQ(nrf24_model_fifo_status(&model) & 0x01, 0);

    nrf24_model_cs_low(&model);
    nrf24_model_xfer(&model, NRF_CMD_R_RX_PAYLOAD);
    ASSERT_EQ(nrf24_model_xfer(&model, NRF_CMD_NOP), 0x11);
    nrf24_model_cs_high(&model);
    ASSERT_EQ(model.rx_count, 1);
    ASSERT_EQ(model.rx_fifo[0][0], 0x22);
}

TEST(test_config_writes_wait_for_standby)
{
    setup();
    nrf24_t dev;
    static const uint8_t addr[5] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 8), NRF24_OK);
    ASSERT_EQ(nrf24_set_mode(&dev, NRF24_MODE_RX), NRF24_OK);

    ASSERT_EQ(nrf24_set_tx_addr(&dev, addr, 5), NRF24_OK);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 1, addr, 5), NRF24_OK);
    ASSERT_EQ(nrf24_set_data_rate(&dev, NRF24_RATE_1MBPS), NRF24_OK);
    ASSERT_EQ(nrf24_set_power(&dev, NRF24_POWER_0DBM), NRF24_OK);
    ASSERT_TRUE(model.ce_high);  /* back to listening */

    ASSERT_EQ(nrf24_set_mode(&dev, NRF24_MODE_TX), NRF24_OK);
    ASSERT_EQ(model.writes_in_rx, 0);
}

/* --- the model itself --- */

/* Register map, p57 to p61 */
TEST(test_model_reset_values)
{
    setup();
    uint8_t a[5];
    static const uint8_t e7[5] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };
    static const uint8_t c2[5] = { 0xC2, 0xC2, 0xC2, 0xC2, 0xC2 };

    ASSERT_EQ_HEX(spi_read(NRF_REG_CONFIG), 0x08);
    ASSERT_EQ_HEX(spi_read(NRF_REG_SETUP_RETR), 0x03);
    ASSERT_EQ_HEX(spi_read(NRF_REG_FIFO_STATUS), 0x11);
    ASSERT_EQ_HEX(spi_status(), 0x0E);

    spi_read_multi(NRF_REG_RX_ADDR_P0, a, 5);
    ASSERT_MEMEQ_N(a, e7, 5);
    spi_read_multi(NRF_REG_RX_ADDR_P1, a, 5);
    ASSERT_MEMEQ_N(a, c2, 5);
    spi_read_multi(NRF_REG_TX_ADDR, a, 5);
    ASSERT_MEMEQ_N(a, e7, 5);
    ASSERT_EQ_HEX(spi_read(0x0C), 0xC3);
    ASSERT_EQ_HEX(spi_read(0x0D), 0xC4);
    ASSERT_EQ_HEX(spi_read(0x0E), 0xC5);
    ASSERT_EQ_HEX(spi_read(0x0F), 0xC6);
}

/* OBSERVE_TX, RPD and FIFO_STATUS flags are type R, p59 and p61 */
TEST(test_model_read_only_registers)
{
    setup();
    power_up(0);
    spi_write(0x08, 0xFF);
    spi_write(0x09, 0xFF);
    spi_write(NRF_REG_FIFO_STATUS, 0x00);
    ASSERT_EQ_HEX(spi_read(0x08), 0x00);
    ASSERT_EQ_HEX(spi_read(0x09), 0x00);
    ASSERT_EQ_HEX(spi_read(NRF_REG_FIFO_STATUS), 0x11);
}

/* "Power on reset 100ms" comes before Power Down, Figure 4 on p22 */
TEST(test_model_takes_no_write_before_power_on_reset_ends)
{
    setup();
    nrf24_model_delay_us(&model, 99999);
    spi_write(NRF_REG_RF_CH, 40);
    ASSERT_EQ_HEX(spi_read(NRF_REG_RF_CH), 0x02);

    nrf24_model_delay_us(&model, 1);
    spi_write(NRF_REG_RF_CH, 40);
    ASSERT_EQ_HEX(spi_read(NRF_REG_RF_CH), 40);
}

/* "must have the PWR_UP bit set high", p23 */
TEST(test_model_tx_needs_pwr_up)
{
    setup();
    nrf24_model_delay_us(&model, 100000);
    spi_upload(0x55);
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_EQ(model.tx_count, 0);
    ASSERT_EQ(spi_status() & NRF_STATUS_TX_DS, 0);
    ASSERT_EQ(spi_read(NRF_REG_FIFO_STATUS) & 0x10, 0);
}

/* "a delay of Tpd2stby ... before the CE is set high", p24 */
TEST(test_model_tx_needs_tpd2stby_before_ce)
{
    setup();
    nrf24_model_delay_us(&model, 100000);
    spi_write(NRF_REG_CONFIG, 0x0A);
    spi_upload(0x55);
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_EQ(model.tx_count, 0);
    ASSERT_EQ(model.ce_before_standby, 1);
}

/* "a high pulse on the CE for more than 10us", p23; Thce 10us, p24 */
TEST(test_model_tx_needs_a_10us_ce_pulse)
{
    setup();
    power_up(0);
    spi_upload(0x55);

    ce_pulse(9);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_EQ(model.tx_count, 0);
    ASSERT_EQ(spi_status() & NRF_STATUS_TX_DS, 0);

    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_EQ(model.tx_count, 1);
    ASSERT_TRUE(spi_status() & NRF_STATUS_TX_DS);
    ASSERT_TRUE(spi_read(NRF_REG_FIFO_STATUS) & 0x10);
}

/* Tstby2a 130us, p24: no flag can appear before the radio has settled */
TEST(test_model_tx_result_comes_after_settling)
{
    setup();
    power_up(0);
    spi_upload(0x55);
    ce_pulse(10);
    ASSERT_EQ(spi_status() & NRF_STATUS_TX_DS, 0);
    nrf24_model_delay_us(&model, 129);
    ASSERT_EQ(spi_status() & NRF_STATUS_TX_DS, 0);
    nrf24_model_delay_us(&model, 1);
    ASSERT_TRUE(spi_status() & NRF_STATUS_TX_DS);
}

/* Three levels with TX_FULL in STATUS and FIFO_STATUS (p56, p59, p61);
 * a CE pulse "will empty one level" (Table 15, p24) */
TEST(test_model_tx_fifo_has_three_levels)
{
    setup();
    power_up(0);
    spi_upload(1);
    spi_upload(2);
    ASSERT_EQ(spi_status() & 0x01, 0);
    ASSERT_EQ(spi_read(NRF_REG_FIFO_STATUS) & 0x20, 0);
    spi_upload(3);
    ASSERT_EQ_HEX(spi_status() & 0x01, 0x01);
    ASSERT_EQ_HEX(spi_read(NRF_REG_FIFO_STATUS) & 0x30, 0x20);

    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_EQ(model.tx_count, 1);
    ASSERT_EQ(spi_status() & 0x01, 0);
    ASSERT_EQ_HEX(spi_read(NRF_REG_FIFO_STATUS) & 0x30, 0x00);
}

/* "The payload in TX FIFO in a PTX is not removed if the MAX_RT IRQ is
 * asserted" (p56); "it must be cleared to enable further communication"
 * (p59) */
TEST(test_model_max_rt_keeps_the_payload_and_blocks_tx)
{
    setup();
    power_up(0);
    spi_upload(0x55);
    model.force_max_rt = true;
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_TRUE(spi_status() & NRF_STATUS_MAX_RT);
    ASSERT_EQ(spi_read(NRF_REG_FIFO_STATUS) & 0x10, 0);

    model.force_max_rt = false;
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_EQ(spi_status() & NRF_STATUS_TX_DS, 0);

    spi_write(NRF_REG_STATUS, NRF_STATUS_MAX_RT);
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_TRUE(spi_status() & NRF_STATUS_TX_DS);
    ASSERT_TRUE(spi_read(NRF_REG_FIFO_STATUS) & 0x10);
}

/* "ARD must be 500uS or more" at 250 kbps, p63 note a: at 250 us the PTX
 * retransmits before the ACK can arrive */
TEST(test_model_250kbps_at_250us_ard_gets_no_ack)
{
    setup();
    power_up(0);
    spi_write(NRF_REG_RF_SETUP, 0x26);
    spi_upload(0x55);
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_TRUE(spi_status() & NRF_STATUS_MAX_RT);
    ASSERT_EQ(spi_status() & NRF_STATUS_TX_DS, 0);

    spi_write(NRF_REG_STATUS, NRF_STATUS_MAX_RT);
    spi_write(NRF_REG_SETUP_RETR, 0x13);
    ce_pulse(10);
    nrf24_model_delay_us(&model, 1000);
    ASSERT_TRUE(spi_status() & NRF_STATUS_TX_DS);
}

/* W_REGISTER is "executable in power down or standby modes only", p51 */
TEST(test_model_ignores_register_writes_in_rx)
{
    static const uint8_t one[4] = { 1, 2, 3, 4 };
    setup();
    listen_on_pipe1(4);

    spi_write(NRF_REG_RF_CH, 40);
    ASSERT_EQ_HEX(spi_read(NRF_REG_RF_CH), 0x02);
    ASSERT_EQ(model.writes_in_rx, 1);

    /* The flags have to stay clearable while listening, p63 */
    nrf24_model_inject_rx_pipe(&model, 1, one, 4);
    ASSERT_TRUE(spi_status() & NRF_STATUS_RX_DR);
    spi_write(NRF_REG_STATUS, NRF_STATUS_RX_DR);
    ASSERT_EQ(spi_status() & NRF_STATUS_RX_DR, 0);

    nrf24_model_ce_low(&model);
    spi_write(NRF_REG_RF_CH, 40);
    ASSERT_EQ_HEX(spi_read(NRF_REG_RF_CH), 40);
}

/* RX_P_NO and TX_FULL are type R, and RX_P_NO names the pipe of the
 * payload available for reading, 111 when empty, p59 */
TEST(test_model_rx_p_no_follows_the_fifo)
{
    static const uint8_t one[4] = { 1, 2, 3, 4 };
    uint8_t buf[4];
    setup();
    power_up(1);
    spi_write(NRF_REG_RX_PW_P0, 4);
    spi_write(NRF_REG_RX_PW_P0 + 1, 4);
    nrf24_model_ce_high(&model);

    spi_write(NRF_REG_STATUS, 0x7F);
    ASSERT_EQ_HEX(spi_status(), 0x0E);

    nrf24_model_inject_rx_pipe(&model, 1, one, 4);
    nrf24_model_inject_rx_pipe(&model, 0, one, 4);
    ASSERT_EQ_HEX(spi_status() & NRF_STATUS_RX_P_NO, 0x02);

    spi_read_payload(buf, 4);
    ASSERT_EQ_HEX(spi_status() & NRF_STATUS_RX_P_NO, 0x00);
    spi_read_payload(buf, 4);
    ASSERT_EQ_HEX(spi_status() & NRF_STATUS_RX_P_NO, 0x0E);
}

/* "If the RX FIFOs are full, the received packet is discarded", p23 */
TEST(test_model_full_rx_fifo_discards)
{
    static const uint8_t p[4] = { 1, 2, 3, 4 };
    setup();
    listen_on_pipe1(4);
    for (int i = 0; i < 4; i++) {
        nrf24_model_inject_rx_pipe(&model, 1, p, 4);
    }
    ASSERT_EQ(model.rx_count, 3);
    ASSERT_EQ_HEX(spi_read(NRF_REG_FIFO_STATUS) & 0x03, 0x02);
}

/* RX_PW_Px: "0 Pipe not used" (p60), and the sender's length "must equal
 * the value in the RX_PW_Px register" (p29) */
TEST(test_model_pipe_takes_only_its_width)
{
    static const uint8_t p[4] = { 1, 2, 3, 4 };
    setup();
    listen_on_pipe1(4);

    nrf24_model_inject_rx_pipe(&model, 0, p, 4);  /* RX_PW_P0 still 0 */
    ASSERT_EQ(model.rx_count, 0);
    nrf24_model_inject_rx_pipe(&model, 1, p, 3);  /* wrong length */
    ASSERT_EQ(model.rx_count, 0);
    nrf24_model_inject_rx_pipe(&model, 2, p, 4);  /* not in EN_RXADDR */
    ASSERT_EQ(model.rx_count, 0);
    ASSERT_EQ(spi_status() & NRF_STATUS_RX_DR, 0);

    nrf24_model_inject_rx_pipe(&model, 1, p, 4);
    ASSERT_EQ(model.rx_count, 1);
}

/* "To enter this mode ... PWR_UP bit, PRIM_RX bit and the CE pin set
 * high", p23 */
TEST(test_model_hears_nothing_outside_rx_mode)
{
    static const uint8_t p[4] = { 1, 2, 3, 4 };
    setup();
    listen_on_pipe1(4);
    nrf24_model_ce_low(&model);
    nrf24_model_inject_rx_pipe(&model, 1, p, 4);
    ASSERT_EQ(model.rx_count, 0);
}

/* --- the driver on the corrected model --- */

/* TX mode needs "PRIM_RX bit set low" (p23). In RX mode a CE pulse sends
 * nothing and only drops the chip out of RX. */
TEST(test_send_is_refused_while_listening)
{
    setup();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 8), NRF24_OK);
    ASSERT_EQ(nrf24_set_mode(&dev, NRF24_MODE_RX), NRF24_OK);

    uint8_t data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ASSERT_EQ(nrf24_send(&dev, data, 8), NRF24_ERR_PARAM);
    ASSERT_TRUE(model.ce_high);
    ASSERT_TRUE(spi_read(NRF_REG_FIFO_STATUS) & 0x10);
}

TEST(test_send_again_after_max_rt)
{
    setup();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 8), NRF24_OK);

    uint8_t data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    model.force_max_rt = true;
    ASSERT_EQ(nrf24_send(&dev, data, 8), NRF24_ERR_MAX_RT);
    ASSERT_EQ(spi_status() & NRF_STATUS_MAX_RT, 0);
    ASSERT_TRUE(spi_read(NRF_REG_FIFO_STATUS) & 0x10);

    model.force_max_rt = false;
    ASSERT_EQ(nrf24_send(&dev, data, 8), NRF24_OK);
}

TEST(test_send_at_250kbps_gets_its_ack)
{
    setup();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 8), NRF24_OK);
    ASSERT_EQ(nrf24_set_data_rate(&dev, NRF24_RATE_250KBPS), NRF24_OK);

    uint8_t data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ASSERT_EQ(nrf24_send(&dev, data, 8), NRF24_OK);
}

TEST(test_receive_on_pipe_two_in_fifo_order)
{
    static const uint8_t lsb = 0xC9;
    static const uint8_t one[4] = { 1, 1, 1, 1 };
    static const uint8_t two[4] = { 2, 2, 2, 2 };
    setup();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &model_spi, 76, 4), NRF24_OK);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 2, &lsb, 1), NRF24_OK);
    ASSERT_EQ(nrf24_set_mode(&dev, NRF24_MODE_RX), NRF24_OK);

    nrf24_model_inject_rx_pipe(&model, 2, one, 4);
    nrf24_model_inject_rx_pipe(&model, 0, two, 4);

    uint8_t buf[4];
    uint8_t len = 0;
    ASSERT_EQ(nrf24_receive(&dev, buf, 4, &len), NRF24_OK);
    ASSERT_EQ(len, 4);
    ASSERT_MEMEQ_N(buf, one, 4);
    ASSERT_TRUE(nrf24_data_ready(&dev));
    ASSERT_EQ(nrf24_receive(&dev, buf, 4, &len), NRF24_OK);
    ASSERT_MEMEQ_N(buf, two, 4);
    ASSERT_FALSE(nrf24_data_ready(&dev));
    ASSERT_EQ(model.writes_in_rx, 0);
}

TEST_MAIN("nrf24l01 hardware model")
    RUN(test_model_reset_values);
    RUN(test_model_read_only_registers);
    RUN(test_model_takes_no_write_before_power_on_reset_ends);
    RUN(test_model_tx_needs_pwr_up);
    RUN(test_model_tx_needs_tpd2stby_before_ce);
    RUN(test_model_tx_needs_a_10us_ce_pulse);
    RUN(test_model_tx_result_comes_after_settling);
    RUN(test_model_tx_fifo_has_three_levels);
    RUN(test_model_max_rt_keeps_the_payload_and_blocks_tx);
    RUN(test_model_250kbps_at_250us_ard_gets_no_ack);
    RUN(test_model_ignores_register_writes_in_rx);
    RUN(test_model_rx_p_no_follows_the_fifo);
    RUN(test_model_full_rx_fifo_discards);
    RUN(test_model_pipe_takes_only_its_width);
    RUN(test_model_hears_nothing_outside_rx_mode);
    RUN(test_send_is_refused_while_listening);
    RUN(test_send_again_after_max_rt);
    RUN(test_send_at_250kbps_gets_its_ack);
    RUN(test_receive_on_pipe_two_in_fifo_order);
    RUN(test_init_success);
    RUN(test_init_writes_config);
    RUN(test_init_sets_channel);
    RUN(test_init_sets_payload_size);
    RUN(test_init_rejects_bad_channel);
    RUN(test_init_rejects_bad_payload);
    RUN(test_set_mode_rx);
    RUN(test_set_mode_tx);
    RUN(test_set_data_rate_250kbps);
    RUN(test_set_power_0dbm);
    RUN(test_send_success);
    RUN(test_send_max_rt);
    RUN(test_receive_data);
    RUN(test_flush_tx);
    RUN(test_read_status);
    RUN(test_model_address_writes_leave_rx_pw_alone);
    RUN(test_model_rx_fifo_pops_on_read);
    RUN(test_config_writes_wait_for_standby);
TEST_MAIN_END()
