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

    /* Put data in TX FIFO via model */
    model.tx_fifo_has_data = true;
    model.tx_fifo_len = 8;

    nrf24_err_t err = nrf24_flush_tx(&dev);
    ASSERT_EQ(err, NRF24_OK);
    ASSERT_FALSE(model.tx_fifo_has_data);
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

TEST_MAIN("nrf24l01 hardware model")
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
TEST_MAIN_END()
