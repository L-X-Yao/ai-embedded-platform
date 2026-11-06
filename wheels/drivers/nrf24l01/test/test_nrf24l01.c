// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "nrf24l01.h"

/* ---------- nRF24L01+ register/command constants for mock ---------- */

#define CMD_R_REGISTER      0x00
#define CMD_W_REGISTER      0x20
#define CMD_R_RX_PAYLOAD    0x61
#define CMD_W_TX_PAYLOAD    0xA0
#define CMD_FLUSH_TX        0xE1
#define CMD_FLUSH_RX        0xE2
#define CMD_NOP             0xFF

#define REG_CONFIG          0x00
#define REG_EN_AA           0x01
#define REG_EN_RXADDR       0x02
#define REG_SETUP_AW        0x03
#define REG_RF_CH           0x05
#define REG_RF_SETUP        0x06
#define REG_STATUS          0x07
#define REG_RX_ADDR_P0      0x0A
#define REG_RX_ADDR_P1      0x0B
#define REG_RX_ADDR_P2      0x0C
#define REG_TX_ADDR         0x10
#define REG_RX_PW_P0        0x11
#define REG_FIFO_STATUS     0x17

#define CONFIG_EN_CRC       0x08
#define CONFIG_CRCO         0x04
#define CONFIG_PWR_UP       0x02
#define CONFIG_PRIM_RX      0x01

#define STATUS_RX_DR        0x40
#define STATUS_TX_DS        0x20
#define STATUS_MAX_RT       0x10

#define RF_SETUP_RF_DR_LOW  0x20
#define RF_SETUP_RF_DR_HIGH 0x08
#define RF_SETUP_RF_PWR_MASK 0x06

/* ---------- mock SPI state ---------- */

#define MOCK_REG_COUNT      0x1E
#define MOCK_ADDR_SIZE      5
#define MOCK_FIFO_DEPTH     3

static uint8_t mock_regs[MOCK_REG_COUNT];
static uint8_t mock_tx_addr[MOCK_ADDR_SIZE];
static uint8_t mock_rx_addr_p0[MOCK_ADDR_SIZE];
static uint8_t mock_rx_addr_p1[MOCK_ADDR_SIZE];
static uint8_t mock_rx_fifo[MOCK_FIFO_DEPTH][32];
static uint8_t mock_rx_fifo_len[MOCK_FIFO_DEPTH];
static uint8_t mock_rx_queued;       /* payloads in the RX FIFO */
static uint8_t mock_tx_payload[32];  /* the last upload, as clocked in */
static uint8_t mock_tx_payload_len;
static uint8_t mock_tx_level;        /* payloads in the TX FIFO */
static int     mock_ce_state;
static int     mock_cs_state;
static int     mock_ce_pulse_count;
static uint32_t mock_delay_total_us;
static int     mock_writes_in_rx;    /* W_REGISTER other than STATUS in RX */

/* SPI transaction tracking */
static uint8_t mock_cur_cmd;
static int     mock_byte_idx;    /* byte index within current transaction */
static int     mock_in_tx;       /* inside CS low..high */

/* Control flags */
static int     mock_force_max_rt;    /* STATUS.MAX_RT on next poll */
static int     mock_force_tx_ds;     /* STATUS.TX_DS on next poll */
static int     mock_poll_count;      /* track status polls */

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    memset(mock_tx_addr, 0, sizeof(mock_tx_addr));
    memset(mock_rx_addr_p0, 0, sizeof(mock_rx_addr_p0));
    memset(mock_rx_addr_p1, 0, sizeof(mock_rx_addr_p1));
    memset(mock_rx_fifo, 0, sizeof(mock_rx_fifo));
    memset(mock_rx_fifo_len, 0, sizeof(mock_rx_fifo_len));
    mock_rx_queued = 0;
    memset(mock_tx_payload, 0, sizeof(mock_tx_payload));
    mock_tx_payload_len = 0;
    mock_tx_level = 0;
    mock_writes_in_rx = 0;
    mock_ce_state = 0;
    mock_cs_state = 1;
    mock_ce_pulse_count = 0;
    mock_delay_total_us = 0;
    mock_cur_cmd = 0;
    mock_byte_idx = 0;
    mock_in_tx = 0;
    mock_force_max_rt = 0;
    mock_force_tx_ds = 0;
    mock_poll_count = 0;

    /* Power-on defaults per datasheet */
    mock_regs[REG_CONFIG]    = 0x08;  /* EN_CRC */
    mock_regs[REG_EN_AA]     = 0x3F;  /* all pipes auto-ack */
    mock_regs[REG_EN_RXADDR] = 0x03;  /* pipe 0,1 enabled */
    mock_regs[REG_SETUP_AW]  = 0x03;  /* 5-byte addr */
    mock_regs[REG_RF_CH]     = 0x02;  /* channel 2 */
    mock_regs[REG_RF_SETUP]  = 0x0E;  /* 2Mbps, 0dBm */
    mock_regs[REG_STATUS]    = 0x0E;  /* RX_P_NO=111, all flags clear */
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs_state = 0;
    mock_in_tx = 1;
    mock_byte_idx = 0;
    mock_cur_cmd = 0;
}

/* A packet arrives: RX_DR rises and the payload takes a level */
static void mock_queue_rx(const uint8_t *data, uint8_t len)
{
    if (mock_rx_queued == MOCK_FIFO_DEPTH) return;
    memcpy(mock_rx_fifo[mock_rx_queued], data, len);
    mock_rx_fifo_len[mock_rx_queued] = len;
    mock_rx_queued++;
    mock_regs[REG_STATUS] = (uint8_t)(mock_regs[REG_STATUS] | STATUS_RX_DR);
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_cs_state = 1;
    mock_in_tx = 0;

    /* "Payload is deleted from FIFO after it is read", p51 */
    if (mock_cur_cmd == CMD_R_RX_PAYLOAD && mock_byte_idx > 1 && mock_rx_queued > 0) {
        mock_rx_queued--;
        for (uint8_t i = 0; i < mock_rx_queued; i++) {
            memcpy(mock_rx_fifo[i], mock_rx_fifo[i + 1], sizeof(mock_rx_fifo[i]));
            mock_rx_fifo_len[i] = mock_rx_fifo_len[i + 1];
        }
    }
    if (mock_cur_cmd == CMD_W_TX_PAYLOAD && mock_byte_idx > 1 &&
        mock_tx_level < MOCK_FIFO_DEPTH) {
        mock_tx_level++;
    }
}

static void mock_ce_low(void *ctx)
{
    (void)ctx;
    if (mock_ce_state == 1) {
        mock_ce_pulse_count++;
    }
    mock_ce_state = 0;
}

static void mock_ce_high(void *ctx)
{
    (void)ctx;
    mock_ce_state = 1;
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    mock_delay_total_us += us;

    /* During the CE pulse of a send, set TX_DS or MAX_RT. TX_DS takes the
     * payload out of the FIFO, MAX_RT leaves it there (p56). */
    if (mock_ce_state == 0 || mock_tx_level == 0) return;
    if (mock_force_tx_ds) {
        mock_regs[REG_STATUS] = (uint8_t)(mock_regs[REG_STATUS] | STATUS_TX_DS);
        mock_tx_level--;
        mock_force_tx_ds = 0;
    }
    if (mock_force_max_rt) {
        mock_regs[REG_STATUS] = (uint8_t)(mock_regs[REG_STATUS] | STATUS_MAX_RT);
        mock_force_max_rt = 0;
    }
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    if (mock_byte_idx == 0) {
        /* First byte = command */
        mock_cur_cmd = tx;
        mock_byte_idx = 1;

        /* Handle single-byte commands */
        if (tx == CMD_FLUSH_TX) mock_tx_level = 0;
        if (tx == CMD_FLUSH_RX) mock_rx_queued = 0;

        /* All commands return STATUS on first byte */
        return mock_regs[REG_STATUS];
    }

    /* Subsequent bytes depend on command */
    uint8_t cmd_type = (uint8_t)(mock_cur_cmd & 0xE0);
    uint8_t reg_addr = (uint8_t)(mock_cur_cmd & 0x1F);
    int data_idx = mock_byte_idx - 1;  /* 0-based data index */
    mock_byte_idx++;

    /* NOP: just return status */
    if (mock_cur_cmd == CMD_NOP) {
        return mock_regs[REG_STATUS];
    }

    /* Read register */
    if (cmd_type == CMD_R_REGISTER) {
        /* Multi-byte register reads */
        if (reg_addr == REG_TX_ADDR) {
            return (data_idx < MOCK_ADDR_SIZE) ? mock_tx_addr[data_idx] : 0;
        }
        if (reg_addr == REG_RX_ADDR_P0) {
            return (data_idx < MOCK_ADDR_SIZE) ? mock_rx_addr_p0[data_idx] : 0;
        }
        if (reg_addr == REG_RX_ADDR_P1) {
            return (data_idx < MOCK_ADDR_SIZE) ? mock_rx_addr_p1[data_idx] : 0;
        }
        if (reg_addr == REG_FIFO_STATUS) {
            uint8_t fifo = 0;
            if (mock_rx_queued == 0) fifo |= 0x01;
            if (mock_rx_queued == MOCK_FIFO_DEPTH) fifo |= 0x02;
            if (mock_tx_level == 0) fifo |= 0x10;
            if (mock_tx_level == MOCK_FIFO_DEPTH) fifo |= 0x20;
            return fifo;
        }
        /* Single-byte register */
        if (reg_addr < MOCK_REG_COUNT) {
            return mock_regs[reg_addr];
        }
        return 0;
    }

    /* Write register */
    if (cmd_type == CMD_W_REGISTER) {
        /* "Executable in power down or standby modes only", p51 */
        if (mock_ce_state == 1 && reg_addr != REG_STATUS &&
            (mock_regs[REG_CONFIG] & CONFIG_PRIM_RX)) {
            mock_writes_in_rx++;
            return 0;
        }
        /* Multi-byte register writes */
        if (reg_addr == REG_TX_ADDR) {
            if (data_idx < MOCK_ADDR_SIZE) mock_tx_addr[data_idx] = tx;
            return 0;
        }
        if (reg_addr == REG_RX_ADDR_P0) {
            if (data_idx < MOCK_ADDR_SIZE) mock_rx_addr_p0[data_idx] = tx;
            return 0;
        }
        if (reg_addr == REG_RX_ADDR_P1) {
            if (data_idx < MOCK_ADDR_SIZE) mock_rx_addr_p1[data_idx] = tx;
            return 0;
        }
        /* Single-byte register */
        if (reg_addr < MOCK_REG_COUNT) {
            if (reg_addr == REG_STATUS) {
                /* Writing 1 clears flag bits, RX_P_NO is read-only */
                mock_regs[REG_STATUS] = (uint8_t)(mock_regs[REG_STATUS] &
                                                  (uint8_t)~(tx & 0x70));
            } else {
                mock_regs[reg_addr] = tx;
            }
        }
        return 0;
    }

    /* Write TX payload */
    if (mock_cur_cmd == CMD_W_TX_PAYLOAD) {
        if (data_idx < 32) {
            mock_tx_payload[data_idx] = tx;
            mock_tx_payload_len = (uint8_t)(data_idx + 1);
        }
        return 0;
    }

    /* Read RX payload */
    if (mock_cur_cmd == CMD_R_RX_PAYLOAD) {
        if (mock_rx_queued > 0 && data_idx < mock_rx_fifo_len[0]) {
            return mock_rx_fifo[0][data_idx];
        }
        return 0;
    }

    return 0xFF;
}

static const nrf24_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .ce_low   = mock_ce_low,
    .ce_high  = mock_ce_high,
    .delay_us = mock_delay_us,
    .ctx      = NULL,
};

/* ================================================================== */
/*                           TESTS                                     */
/* ================================================================== */

/* 1. Init: verify CONFIG register written (PWR_UP, EN_CRC) */
TEST(test_init_ok)
{
    mock_reset();
    nrf24_t dev;
    nrf24_err_t e = nrf24_init(&dev, &mock_spi, 76, 16);
    ASSERT_EQ(e, NRF24_OK);

    /* CONFIG should have PWR_UP + EN_CRC + CRCO set */
    uint8_t cfg = mock_regs[REG_CONFIG];
    ASSERT_TRUE(cfg & CONFIG_PWR_UP);
    ASSERT_TRUE(cfg & CONFIG_EN_CRC);
    ASSERT_TRUE(cfg & CONFIG_CRCO);
    /* TX mode: PRIM_RX should be 0 */
    ASSERT_FALSE(cfg & CONFIG_PRIM_RX);

    /* Channel should be set */
    ASSERT_EQ(mock_regs[REG_RF_CH], 76);

    /* Payload size for pipe 0 */
    ASSERT_EQ(mock_regs[REG_RX_PW_P0], 16);

    /* Device state */
    ASSERT_EQ(dev.channel, 76);
    ASSERT_EQ(dev.payload_size, 16);
    ASSERT_EQ(dev.mode, NRF24_MODE_TX);
}

/* 2. Init NULL -> error */
TEST(test_init_null_dev)
{
    ASSERT_EQ(nrf24_init(NULL, &mock_spi, 0, 1), NRF24_ERR_SPI);
}

TEST(test_init_null_spi)
{
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, NULL, 0, 1), NRF24_ERR_SPI);
}

/* 3. Set channel: verify RF_CH register */
TEST(test_set_channel)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 100, 8);
    ASSERT_EQ(mock_regs[REG_RF_CH], 100);
}

/* 4. Set TX address: verify TX_ADDR + RX_ADDR_P0 (auto-ack) */
TEST(test_set_tx_addr)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr[] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
    nrf24_err_t e = nrf24_set_tx_addr(&dev, addr, 5);
    ASSERT_EQ(e, NRF24_OK);

    /* TX_ADDR should match */
    ASSERT_TRUE(memcmp(mock_tx_addr, addr, 5) == 0);
    /* RX_ADDR_P0 should also match (for auto-ack) */
    ASSERT_TRUE(memcmp(mock_rx_addr_p0, addr, 5) == 0);
    /* SETUP_AW = addr_len - 2 = 3 */
    ASSERT_EQ(mock_regs[REG_SETUP_AW], 3);
}

/* 5. Set RX address pipe 0 */
TEST(test_set_rx_addr_pipe0)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr[] = {0xC2, 0xC2, 0xC2, 0xC2, 0xC2};
    nrf24_err_t e = nrf24_set_rx_addr(&dev, 0, addr, 5);
    ASSERT_EQ(e, NRF24_OK);

    ASSERT_TRUE(memcmp(mock_rx_addr_p0, addr, 5) == 0);

    /* Pipe 0 should be enabled in EN_RXADDR */
    ASSERT_TRUE(mock_regs[REG_EN_RXADDR] & 0x01);
}

/* 6. Set data rate: verify RF_SETUP bits */
TEST(test_set_data_rate_250kbps)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    nrf24_err_t e = nrf24_set_data_rate(&dev, NRF24_RATE_250KBPS);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_TRUE(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_DR_LOW);
    ASSERT_FALSE(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_DR_HIGH);
}

TEST(test_set_data_rate_1mbps)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    nrf24_err_t e = nrf24_set_data_rate(&dev, NRF24_RATE_1MBPS);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_FALSE(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_DR_LOW);
    ASSERT_FALSE(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_DR_HIGH);
}

TEST(test_set_data_rate_2mbps)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    nrf24_err_t e = nrf24_set_data_rate(&dev, NRF24_RATE_2MBPS);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_FALSE(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_DR_LOW);
    ASSERT_TRUE(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_DR_HIGH);
}

/* 7. Set power: verify RF_SETUP bits */
TEST(test_set_power)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    /* Set -18dBm (0b00 in bits [2:1]) */
    nrf24_err_t e = nrf24_set_power(&dev, NRF24_POWER_N18DBM);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_PWR_MASK, 0x00);

    /* Set 0dBm (0b11 in bits [2:1]) */
    e = nrf24_set_power(&dev, NRF24_POWER_0DBM);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_PWR_MASK, 0x06);

    /* Set -6dBm (0b10 in bits [2:1]) */
    e = nrf24_set_power(&dev, NRF24_POWER_N6DBM);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_PWR_MASK, 0x04);
}

/* 8. Set mode TX/RX: verify CONFIG PRIM_RX bit */
TEST(test_set_mode_rx)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    nrf24_err_t e = nrf24_set_mode(&dev, NRF24_MODE_RX);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_TRUE(mock_regs[REG_CONFIG] & CONFIG_PRIM_RX);
    ASSERT_EQ(dev.mode, NRF24_MODE_RX);
    /* CE should be high in RX mode */
    ASSERT_EQ(mock_ce_state, 1);
}

TEST(test_set_mode_tx)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    /* First set RX, then back to TX */
    nrf24_set_mode(&dev, NRF24_MODE_RX);
    nrf24_err_t e = nrf24_set_mode(&dev, NRF24_MODE_TX);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_FALSE(mock_regs[REG_CONFIG] & CONFIG_PRIM_RX);
    ASSERT_EQ(dev.mode, NRF24_MODE_TX);
    /* CE should be low in TX mode */
    ASSERT_EQ(mock_ce_state, 0);
    ASSERT_EQ(mock_writes_in_rx, 0);
}

/* 9. Send: verify W_TX_PAYLOAD command + CE pulse */
TEST(test_send_ok)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 4);
    mock_ce_pulse_count = 0;

    /* Mock: TX_DS flag will be set after CE pulse */
    mock_force_tx_ds = 1;

    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    nrf24_err_t e = nrf24_send(&dev, data, 4);
    ASSERT_EQ(e, NRF24_OK);

    /* Verify payload was written */
    ASSERT_EQ(mock_tx_payload_len, 4);
    ASSERT_EQ(mock_tx_payload[0], 0xDE);
    ASSERT_EQ(mock_tx_payload[3], 0xEF);

    /* CE should have been pulsed (high then low) */
    ASSERT_TRUE(mock_ce_pulse_count > 0);

    /* TX_DS: the payload left the FIFO and the flag is clear again */
    ASSERT_EQ(mock_tx_level, 0);
    ASSERT_FALSE(mock_regs[REG_STATUS] & STATUS_TX_DS);
}

/* 10. Send with MAX_RT -> ERR_MAX_RT */
TEST(test_send_max_rt)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 2);

    /* Mock: MAX_RT flag will be set after CE pulse */
    mock_force_max_rt = 1;

    uint8_t data[] = {0x01, 0x02};
    nrf24_err_t e = nrf24_send(&dev, data, 2);
    ASSERT_EQ(e, NRF24_ERR_MAX_RT);

    /* "It must be cleared to enable further communication" (p59), and the
     * payload MAX_RT left behind must not go out with the next send */
    ASSERT_FALSE(mock_regs[REG_STATUS] & STATUS_MAX_RT);
    ASSERT_EQ(mock_tx_level, 0);
}

/* 11. Receive: mock RX_DR flag + read payload */
TEST(test_receive)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 4);

    /* Put data in RX FIFO mock */
    static const uint8_t pkt[4] = { 0xCA, 0xFE, 0xBA, 0xBE };
    mock_queue_rx(pkt, 4);

    uint8_t buf[32] = {0};
    uint8_t len = 0;
    nrf24_err_t e = nrf24_receive(&dev, buf, 32, &len);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(len, 4);
    ASSERT_EQ(buf[0], 0xCA);
    ASSERT_EQ(buf[1], 0xFE);
    ASSERT_EQ(buf[2], 0xBA);
    ASSERT_EQ(buf[3], 0xBE);

    /* The read took the payload and cleared RX_DR */
    ASSERT_EQ(mock_rx_queued, 0);
    ASSERT_FALSE(mock_regs[REG_STATUS] & STATUS_RX_DR);
}

/* 12. data_ready: check the RX FIFO */
TEST(test_data_ready)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    /* No data ready */
    ASSERT_FALSE(nrf24_data_ready(&dev));

    /* A packet arrives */
    static const uint8_t pkt[16] = { 0 };
    mock_queue_rx(pkt, 16);
    ASSERT_TRUE(nrf24_data_ready(&dev));
}

/* 13. Flush TX/RX: verify commands sent */
TEST(test_flush_tx)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    /* A send that got no answer leaves its payload in the FIFO */
    uint8_t data[16] = { 0 };
    ASSERT_EQ(nrf24_send(&dev, data, 16), NRF24_ERR_TIMEOUT);
    ASSERT_EQ(mock_tx_level, 1);

    nrf24_err_t e = nrf24_flush_tx(&dev);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_tx_level, 0);
}

TEST(test_flush_rx)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    static const uint8_t pkt[16] = { 0 };
    mock_queue_rx(pkt, 16);
    mock_queue_rx(pkt, 16);
    ASSERT_TRUE(nrf24_data_ready(&dev));

    nrf24_err_t e = nrf24_flush_rx(&dev);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_rx_queued, 0);
    ASSERT_FALSE(nrf24_data_ready(&dev));
}

/* 14. Channel range validation (0-125) */
TEST(test_channel_range)
{
    mock_reset();
    nrf24_t dev;

    /* Channel 0: OK */
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 0, 16), NRF24_OK);

    /* Channel 125: OK */
    mock_reset();
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 125, 16), NRF24_OK);

    /* Channel 126: ERR_PARAM */
    mock_reset();
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 126, 16), NRF24_ERR_PARAM);
}

/* 15. Payload size validation (1-32) */
TEST(test_payload_size_range)
{
    mock_reset();
    nrf24_t dev;

    /* Size 0: ERR_PARAM */
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 76, 0), NRF24_ERR_PARAM);

    /* Size 1: OK */
    mock_reset();
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 76, 1), NRF24_OK);

    /* Size 32: OK */
    mock_reset();
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 76, 32), NRF24_OK);

    /* Size 33: ERR_PARAM */
    mock_reset();
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 76, 33), NRF24_ERR_PARAM);
}

/* 16. TX address length validation */
TEST(test_tx_addr_len_validation)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    /* addr_len 2: too short */
    ASSERT_EQ(nrf24_set_tx_addr(&dev, addr, 2), NRF24_ERR_PARAM);

    /* addr_len 3: OK */
    ASSERT_EQ(nrf24_set_tx_addr(&dev, addr, 3), NRF24_OK);

    /* addr_len 6: too long */
    ASSERT_EQ(nrf24_set_tx_addr(&dev, addr, 6), NRF24_ERR_PARAM);
}

/* 17. RX pipe 2-5 single-byte address */
TEST(test_rx_addr_pipe2_single_byte)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr = 0xC3;
    nrf24_err_t e = nrf24_set_rx_addr(&dev, 2, &addr, 1);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RX_ADDR_P2], 0xC3);

    /* Pipe 2 with multi-byte addr: ERR_PARAM */
    uint8_t multi[3] = {0xC3, 0xC4, 0xC5};
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 2, multi, 3), NRF24_ERR_PARAM);
}

/* 18. Pipe validation */
TEST(test_rx_addr_invalid_pipe)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr = 0xC3;
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 6, &addr, 1), NRF24_ERR_PARAM);
}

/* 19. Null safety for all API functions */
TEST(test_null_safety)
{
    nrf24_t dev;
    uint8_t buf[32];
    uint8_t len;
    uint8_t addr[5] = {0};

    mock_reset();
    nrf24_init(&dev, &mock_spi, 76, 16);

    /* NULL dev */
    ASSERT_EQ(nrf24_set_tx_addr(NULL, addr, 5), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_rx_addr(NULL, 0, addr, 5), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_mode(NULL, NRF24_MODE_TX), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_data_rate(NULL, NRF24_RATE_1MBPS), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_power(NULL, NRF24_POWER_0DBM), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_send(NULL, buf, 1), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_receive(NULL, buf, 32, &len), NRF24_ERR_SPI);
    ASSERT_FALSE(nrf24_data_ready(NULL));
    ASSERT_EQ(nrf24_flush_tx(NULL), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_flush_rx(NULL), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_read_status(NULL), 0);

    /* NULL data pointers */
    ASSERT_EQ(nrf24_set_tx_addr(&dev, NULL, 5), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 0, NULL, 5), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_send(&dev, NULL, 1), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_receive(&dev, NULL, 32, &len), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_receive(&dev, buf, 32, NULL), NRF24_ERR_SPI);
}

/* 20. Send payload length validation */
TEST(test_send_len_validation)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t data[33];
    memset(data, 0x55, sizeof(data));

    /* len 0: ERR_PARAM */
    ASSERT_EQ(nrf24_send(&dev, data, 0), NRF24_ERR_PARAM);

    /* len 33: ERR_PARAM */
    ASSERT_EQ(nrf24_send(&dev, data, 33), NRF24_ERR_PARAM);
}

/* 21. Read status */
TEST(test_read_status)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t st = nrf24_read_status(&dev);
    /* Status flags were cleared during init */
    ASSERT_FALSE(st & STATUS_RX_DR);
    ASSERT_FALSE(st & STATUS_TX_DS);
    ASSERT_FALSE(st & STATUS_MAX_RT);
}

/* 22. Receive with no data ready */
TEST(test_receive_no_data)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t buf[32];
    uint8_t len = 99;
    nrf24_err_t e = nrf24_receive(&dev, buf, 32, &len);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(len, 0);
}

/* 23. 3-byte address */
TEST(test_tx_addr_3byte)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr[] = {0xAA, 0xBB, 0xCC};
    nrf24_err_t e = nrf24_set_tx_addr(&dev, addr, 3);
    ASSERT_EQ(e, NRF24_OK);
    /* SETUP_AW = 3 - 2 = 1 */
    ASSERT_EQ(mock_regs[REG_SETUP_AW], 1);
    ASSERT_EQ(mock_tx_addr[0], 0xAA);
    ASSERT_EQ(mock_tx_addr[1], 0xBB);
    ASSERT_EQ(mock_tx_addr[2], 0xCC);
}

/* 24. Invalid data rate */
TEST(test_invalid_data_rate)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    ASSERT_EQ(nrf24_set_data_rate(&dev, (nrf24_data_rate_t)99), NRF24_ERR_PARAM);
}

/* 25. Invalid power level */
TEST(test_invalid_power)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    ASSERT_EQ(nrf24_set_power(&dev, (nrf24_power_t)99), NRF24_ERR_PARAM);
}

/* 26. Send timeout — neither TX_DS nor MAX_RT ever fires.
 * NRF24_TX_TIMEOUT_US defaults to 10000; mock_delay_us accumulates
 * delay_total but never sets any status bit. */
TEST(test_send_timeout)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 3);

    /* Do NOT set mock_force_tx_ds or mock_force_max_rt — status stays
     * clear, so the poll loop runs until elapsed >= NRF24_TX_TIMEOUT_US. */
    uint8_t data[] = {0x01, 0x02, 0x03};
    nrf24_err_t e = nrf24_send(&dev, data, 3);
    ASSERT_EQ(e, NRF24_ERR_TIMEOUT);
}

/* 27. set_rx_addr pipe 1 (multi-byte, exercises write_reg_multi for P1) */
TEST(test_set_rx_addr_pipe1)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr[] = {0xC2, 0xC3, 0xC4, 0xC5, 0xC6};
    nrf24_err_t e = nrf24_set_rx_addr(&dev, 1, addr, 5);
    ASSERT_EQ(e, NRF24_OK);

    /* Pipe 1 address should be in mock_rx_addr_p1 */
    ASSERT_TRUE(memcmp(mock_rx_addr_p1, addr, 5) == 0);

    /* Pipe 1 should be enabled in EN_RXADDR */
    ASSERT_TRUE(mock_regs[REG_EN_RXADDR] & 0x02);
}

/* 28. set_rx_addr pipe 0/1 with too-short address (addr_len < 3) */
TEST(test_rx_addr_pipe0_short_addr)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t addr[2] = {0xAA, 0xBB};
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 0, addr, 2), NRF24_ERR_PARAM);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 1, addr, 2), NRF24_ERR_PARAM);
}

/* 29. set_rx_addr pipe 3, 4, 5 (single byte, different register offsets) */
TEST(test_set_rx_addr_pipes_3_to_5)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    uint8_t a3 = 0xD3;
    uint8_t a4 = 0xD4;
    uint8_t a5 = 0xD5;

    ASSERT_EQ(nrf24_set_rx_addr(&dev, 3, &a3, 1), NRF24_OK);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 4, &a4, 1), NRF24_OK);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 5, &a5, 1), NRF24_OK);

    /* Verify register values */
    ASSERT_EQ(mock_regs[0x0D], 0xD3); /* REG_RX_ADDR_P3 */
    ASSERT_EQ(mock_regs[0x0E], 0xD4); /* REG_RX_ADDR_P4 */
    ASSERT_EQ(mock_regs[0x0F], 0xD5); /* REG_RX_ADDR_P5 */
}

/* 30. receive — payload too large for caller buffer (ERR_PARAM path) */
TEST(test_receive_buf_too_small)
{
    mock_reset();
    nrf24_t dev;
    /* payload_size = 16, but we call receive with max_len = 4 */
    nrf24_init(&dev, &mock_spi, 76, 16);

    static const uint8_t pkt[16] = { 0 };
    mock_queue_rx(pkt, 16);
    uint8_t buf[4];
    uint8_t len = 0;
    nrf24_err_t e = nrf24_receive(&dev, buf, 4, &len);
    ASSERT_EQ(e, NRF24_ERR_PARAM);
    ASSERT_EQ(mock_rx_queued, 1);  /* the payload stays for a bigger buffer */
}

/* 31. set_power -12dBm (missing power level from existing test) */
TEST(test_set_power_n12dbm)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    nrf24_err_t e = nrf24_set_power(&dev, NRF24_POWER_N12DBM);
    ASSERT_EQ(e, NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_PWR_MASK, 0x02);
}

/* ---------- main ---------- */

TEST(test_receive_reaches_every_queued_payload)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 4);

    /* Two payloads under one RX_DR: clearing it must not strand the second */
    static const uint8_t one[4] = { 1, 1, 1, 1 };
    static const uint8_t two[4] = { 2, 2, 2, 2 };
    mock_queue_rx(one, 4);
    mock_queue_rx(two, 4);
    uint8_t buf[32];
    uint8_t len = 0;
    ASSERT_EQ(nrf24_receive(&dev, buf, 32, &len), NRF24_OK);
    ASSERT_EQ(len, 4);
    ASSERT_MEMEQ_N(buf, one, 4);
    ASSERT_TRUE(nrf24_data_ready(&dev));

    len = 0;
    ASSERT_EQ(nrf24_receive(&dev, buf, 32, &len), NRF24_OK);
    ASSERT_EQ(len, 4);
    ASSERT_MEMEQ_N(buf, two, 4);
    ASSERT_FALSE(nrf24_data_ready(&dev));
}

/* W_REGISTER is "executable in power down or standby modes only" (p51) */
TEST(test_settings_change_while_listening)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);
    ASSERT_EQ(nrf24_set_mode(&dev, NRF24_MODE_RX), NRF24_OK);

    uint8_t addr[5] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(nrf24_set_tx_addr(&dev, addr, 5), NRF24_OK);
    ASSERT_EQ(nrf24_set_data_rate(&dev, NRF24_RATE_1MBPS), NRF24_OK);
    ASSERT_EQ(nrf24_set_power(&dev, NRF24_POWER_N6DBM), NRF24_OK);

    ASSERT_EQ(mock_writes_in_rx, 0);
    ASSERT_TRUE(memcmp(mock_tx_addr, addr, 5) == 0);
    ASSERT_EQ(mock_regs[REG_RF_SETUP] & RF_SETUP_RF_PWR_MASK, 0x04);
    ASSERT_EQ(mock_ce_state, 1);
}

TEST(test_rx_addr_sets_the_pipe_payload_width)
{
    mock_reset();
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    static const uint8_t addr[5] = { 0xC2, 0xC2, 0xC2, 0xC2, 0xC2 };
    static const uint8_t lsb = 0xC6;
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 1, addr, 5), NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RX_PW_P0 + 1], 16);
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 5, &lsb, 1), NRF24_OK);
    ASSERT_EQ(mock_regs[REG_RX_PW_P0 + 5], 16);
}

TEST(test_250kbps_sets_a_retransmit_delay_of_500us)
{
    mock_reset();
    mock_regs[0x04] = 0x03;  /* reset value: 250 us, 3 retries */
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    ASSERT_EQ(nrf24_set_data_rate(&dev, NRF24_RATE_250KBPS), NRF24_OK);
    ASSERT_EQ(mock_regs[0x04], 0x13);
}

TEST(test_init_waits_out_the_power_on_reset)
{
    mock_reset();
    nrf24_t dev;
    ASSERT_EQ(nrf24_init(&dev, &mock_spi, 76, 16), NRF24_OK);
    ASSERT_GE(mock_delay_total_us, 100000u);
}

TEST(test_rx_addr_sets_the_address_width)
{
    mock_reset();
    mock_regs[REG_SETUP_AW] = 0x03;  /* reset value: 5 bytes */
    nrf24_t dev;
    nrf24_init(&dev, &mock_spi, 76, 16);

    static const uint8_t addr[3] = { 0xE7, 0xE7, 0xE7 };
    ASSERT_EQ(nrf24_set_rx_addr(&dev, 1, addr, 3), NRF24_OK);
    ASSERT_EQ(mock_regs[REG_SETUP_AW], 0x01);
}

/* ---------- a device init refused ---------- */

/* This driver gives NRF24_ERR_SPI for a missing device or SPI */
static void assert_device_refused(nrf24_t *d)
{
    static const uint8_t addr[5] = { 1, 2, 3, 4, 5 };
    uint8_t buf[4] = { 0 };
    uint8_t len = 0;
    ASSERT_EQ(nrf24_set_tx_addr(d, addr, 5), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_rx_addr(d, 0, addr, 5), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_mode(d, NRF24_MODE_TX), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_data_rate(d, NRF24_RATE_1MBPS), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_set_power(d, NRF24_POWER_0DBM), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_send(d, buf, 1), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_receive(d, buf, sizeof(buf), &len), NRF24_ERR_SPI);
    ASSERT_FALSE(nrf24_data_ready(d));
    ASSERT_EQ(nrf24_flush_tx(d), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_flush_rx(d), NRF24_ERR_SPI);
    ASSERT_EQ(nrf24_read_status(d), 0);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    nrf24_t d;

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(nrf24_init(&d, NULL, 76, 16), NRF24_ERR_SPI);
    assert_device_refused(&d);

    static nrf24_spi_t zeroed_hal;
    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(nrf24_init(&d, &zeroed_hal, 76, 16), NRF24_ERR_SPI);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(nrf24_init(&d, &mock_spi, 126, 16), NRF24_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("nrf24l01")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_set_channel);
    RUN(test_set_tx_addr);
    RUN(test_set_rx_addr_pipe0);
    RUN(test_set_data_rate_250kbps);
    RUN(test_set_data_rate_1mbps);
    RUN(test_set_data_rate_2mbps);
    RUN(test_set_power);
    RUN(test_set_mode_rx);
    RUN(test_set_mode_tx);
    RUN(test_send_ok);
    RUN(test_send_max_rt);
    RUN(test_receive);
    RUN(test_data_ready);
    RUN(test_flush_tx);
    RUN(test_flush_rx);
    RUN(test_channel_range);
    RUN(test_payload_size_range);
    RUN(test_tx_addr_len_validation);
    RUN(test_rx_addr_pipe2_single_byte);
    RUN(test_rx_addr_invalid_pipe);
    RUN(test_null_safety);
    RUN(test_send_len_validation);
    RUN(test_read_status);
    RUN(test_receive_no_data);
    RUN(test_tx_addr_3byte);
    RUN(test_invalid_data_rate);
    RUN(test_invalid_power);
    RUN(test_send_timeout);
    RUN(test_set_rx_addr_pipe1);
    RUN(test_rx_addr_pipe0_short_addr);
    RUN(test_set_rx_addr_pipes_3_to_5);
    RUN(test_receive_buf_too_small);
    RUN(test_set_power_n12dbm);
    RUN(test_receive_reaches_every_queued_payload);
    RUN(test_settings_change_while_listening);
    RUN(test_rx_addr_sets_the_pipe_payload_width);
    RUN(test_250kbps_sets_a_retransmit_delay_of_500us);
    RUN(test_init_waits_out_the_power_on_reset);
    RUN(test_rx_addr_sets_the_address_width);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
