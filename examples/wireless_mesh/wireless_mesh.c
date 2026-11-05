// SPDX-License-Identifier: MIT
/*
 * wireless_mesh -- integration recipe: wireless sensor mesh network
 *
 * Demonstrates six wheels working together:
 *   L0  slip           -- SLIP framing for inter-node serial comm
 *   L0  checksum       -- packet integrity verification
 *   L0  rle_codec      -- compress sensor payloads before TX
 *   L1  nrf24l01       -- 2.4 GHz short-range radio transceiver
 *   L1  lora_sx1276    -- LoRa long-range radio transceiver
 *   L2  lorawan_packet -- LoRaWAN uplink frame encoding
 *
 * Runs entirely on host with mock SPI -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/wireless_mesh
 */

#include "test_harness.h"

/* wheels */
#include "slip.h"
#include "checksum.h"
#include "rle_codec.h"
#include "nrf24l01.h"
#include "lora_sx1276.h"
#include "lorawan_packet.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define NRF_CHANNEL       76       /* 2476 MHz                          */
#define NRF_PAYLOAD_SIZE  32       /* fixed payload size for nRF24      */
#define LORA_FREQ_HZ      868000000UL  /* 868 MHz EU ISM               */

/* Buffer sizes -- static allocation only */
#define SENSOR_DATA_LEN   32
#define RLE_BUF_SIZE      128
#define SLIP_BUF_SIZE     512
#define RADIO_BUF_SIZE    256

/* ======================================================================
 * Mock SPI -- nRF24L01+
 *
 * Simulates register file + TX/RX FIFO loopback. Data written via
 * W_TX_PAYLOAD is copied to an internal FIFO; when STATUS is read
 * after CE pulse, TX_DS flag is set. Reading R_RX_PAYLOAD returns
 * the loopback data with RX_DR flag.
 * ====================================================================== */

/* nRF24L01+ SPI commands (must match driver) */
#define NRF_CMD_R_REGISTER   0x00
#define NRF_CMD_W_REGISTER   0x20
#define NRF_CMD_R_RX_PAYLOAD 0x61
#define NRF_CMD_W_TX_PAYLOAD 0xA0
#define NRF_CMD_FLUSH_TX     0xE1
#define NRF_CMD_FLUSH_RX     0xE2
#define NRF_CMD_NOP          0xFF

/* nRF24 status bits */
#define NRF_STATUS_RX_DR     0x40
#define NRF_STATUS_TX_DS     0x20
#define NRF_STATUS_MAX_RT    0x10

typedef struct {
    uint8_t regs[128];
    uint8_t tx_fifo[32];
    uint8_t tx_len;
    uint8_t rx_fifo[32];
    uint8_t rx_len;
    bool    ce_is_high;
    bool    tx_pending;    /* payload written, waiting for CE pulse */
    uint8_t spi_cmd;       /* current SPI command in progress */
    uint8_t spi_byte_idx;  /* byte position within multi-byte xfer */
} nrf_mock_state_t;

static nrf_mock_state_t nrf_mock;

static void nrf_mock_reset(void)
{
    memset(&nrf_mock, 0, sizeof(nrf_mock));
    /* STATUS register power-on default */
    nrf_mock.regs[0x07] = 0x0E;
}

static void nrf_cs_low(void *ctx)
{
    (void)ctx;
    nrf_mock.spi_cmd = 0;
    nrf_mock.spi_byte_idx = 0;
}

static void nrf_cs_high(void *ctx)
{
    (void)ctx;
    /* Transaction complete -- if we wrote a TX payload and CE was pulsed,
     * simulate successful transmission by setting TX_DS in STATUS */
    if (nrf_mock.tx_pending && nrf_mock.ce_is_high) {
        nrf_mock.regs[0x07] |= NRF_STATUS_TX_DS;
        /* loopback: copy TX FIFO to RX FIFO */
        memcpy(nrf_mock.rx_fifo, nrf_mock.tx_fifo, nrf_mock.tx_len);
        nrf_mock.rx_len = nrf_mock.tx_len;
        nrf_mock.regs[0x07] |= NRF_STATUS_RX_DR;
        nrf_mock.tx_pending = false;
    }

    /* A payload leaves the RX FIFO once it has been read */
    if (nrf_mock.spi_cmd == NRF_CMD_R_RX_PAYLOAD) {
        nrf_mock.rx_len = 0;
    }
}

static uint8_t nrf_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    uint8_t ret = nrf_mock.regs[0x07]; /* STATUS returned on first byte */

    if (nrf_mock.spi_byte_idx == 0) {
        /* First byte is command */
        nrf_mock.spi_cmd = tx;
        nrf_mock.spi_byte_idx = 1;

        if (tx == NRF_CMD_FLUSH_TX) {
            nrf_mock.tx_len = 0;
            nrf_mock.tx_pending = false;
        } else if (tx == NRF_CMD_FLUSH_RX) {
            nrf_mock.rx_len = 0;
        }
        return ret;
    }

    /* Subsequent bytes depend on command */
    uint8_t idx = (uint8_t)(nrf_mock.spi_byte_idx - 1);
    nrf_mock.spi_byte_idx++;

    if (nrf_mock.spi_cmd == NRF_CMD_NOP) {
        /* NOP: just return status */
        return ret;
    }

    if ((nrf_mock.spi_cmd & 0xE0) == NRF_CMD_R_REGISTER) {
        /* Read register */
        uint8_t reg = nrf_mock.spi_cmd & 0x1F;
        if (reg == 0x17) {
            return nrf_mock.rx_len ? 0x10 : 0x11;  /* FIFO_STATUS: RX_EMPTY */
        }
        return nrf_mock.regs[reg + idx];
    }

    if ((nrf_mock.spi_cmd & 0xE0) == NRF_CMD_W_REGISTER) {
        /* Write register */
        uint8_t reg = nrf_mock.spi_cmd & 0x1F;
        if (reg == 0x07 && idx == 0) {
            /* STATUS register: writing 1 clears the flag (W1C) */
            nrf_mock.regs[0x07] &= (uint8_t)~tx;
        } else {
            nrf_mock.regs[reg + idx] = tx;
        }
        return 0;
    }

    if (nrf_mock.spi_cmd == NRF_CMD_W_TX_PAYLOAD) {
        /* Write TX payload */
        if (idx < 32) {
            nrf_mock.tx_fifo[idx] = tx;
            nrf_mock.tx_len = idx + 1;
            nrf_mock.tx_pending = true;
        }
        return 0;
    }

    if (nrf_mock.spi_cmd == NRF_CMD_R_RX_PAYLOAD) {
        /* Read RX payload */
        if (idx < nrf_mock.rx_len) {
            return nrf_mock.rx_fifo[idx];
        }
        return 0;
    }

    return 0;
}

static void nrf_ce_low(void *ctx)
{
    (void)ctx;
    /* On CE falling edge after TX, mark TX complete */
    if (nrf_mock.ce_is_high && nrf_mock.tx_pending) {
        nrf_mock.regs[0x07] |= NRF_STATUS_TX_DS;
        memcpy(nrf_mock.rx_fifo, nrf_mock.tx_fifo, nrf_mock.tx_len);
        nrf_mock.rx_len = nrf_mock.tx_len;
        nrf_mock.regs[0x07] |= NRF_STATUS_RX_DR;
        nrf_mock.tx_pending = false;
    }
    nrf_mock.ce_is_high = false;
}

static void nrf_ce_high(void *ctx)
{
    (void)ctx;
    nrf_mock.ce_is_high = true;
}

static void nrf_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
}

static const nrf24_spi_t nrf_spi = {
    .cs_low   = nrf_cs_low,
    .cs_high  = nrf_cs_high,
    .xfer     = nrf_xfer,
    .ce_low   = nrf_ce_low,
    .ce_high  = nrf_ce_high,
    .delay_us = nrf_delay_us,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock SPI -- LoRa SX1276
 *
 * Simulates register file. VERSION register returns 0x12 (SX1276).
 * Writing to FIFO (reg 0x00) stores data; reading returns it.
 * OpMode read-back mirrors last write. IRQ_FLAGS simulates TxDone
 * after send and RxDone after receive.
 * ====================================================================== */

/* SX1276 register addresses (must match driver) */
#define LORA_REG_FIFO              0x00
#define LORA_REG_OP_MODE           0x01
#define LORA_REG_IRQ_FLAGS         0x12
#define LORA_REG_RX_NB_BYTES      0x13
#define LORA_REG_FIFO_RX_CURRENT  0x10
#define LORA_REG_PAYLOAD_LEN      0x22
#define LORA_REG_VERSION           0x42
#define LORA_SPI_WRITE             0x80

#define LORA_IRQ_TX_DONE           0x08
#define LORA_IRQ_RX_DONE           0x40

#define LORA_OPMODE_LORA           0x80
#define LORA_MODE_TX               0x03
#define LORA_MODE_RX_SINGLE        0x06

typedef struct {
    uint8_t regs[128];
    uint8_t fifo[256];
    uint8_t fifo_ptr;       /* current FIFO read/write pointer */
    uint8_t fifo_len;       /* bytes written to FIFO */
    bool    tx_active;
    uint8_t spi_addr;       /* register address for current xfer */
    uint8_t spi_byte_idx;
    int     delay_calls;    /* count delay_ms calls for TX/RX done sim */
} lora_mock_state_t;

static lora_mock_state_t lora_mock;

static void lora_mock_reset(void)
{
    memset(&lora_mock, 0, sizeof(lora_mock));
    lora_mock.regs[LORA_REG_VERSION] = 0x12; /* SX1276 chip ID */
    lora_mock.regs[LORA_REG_OP_MODE] = LORA_OPMODE_LORA;
}

static void lora_cs_low(void *ctx)
{
    (void)ctx;
    lora_mock.spi_addr = 0;
    lora_mock.spi_byte_idx = 0;
}

static void lora_cs_high(void *ctx)
{
    (void)ctx;
}

static uint8_t lora_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    if (lora_mock.spi_byte_idx == 0) {
        /* First byte: register address (bit 7 = write flag) */
        lora_mock.spi_addr = tx;
        lora_mock.spi_byte_idx = 1;
        return lora_mock.regs[LORA_REG_OP_MODE]; /* status-like */
    }

    bool is_write = (lora_mock.spi_addr & LORA_SPI_WRITE) != 0;
    uint8_t reg = lora_mock.spi_addr & 0x7F;

    if (reg == LORA_REG_FIFO) {
        if (is_write) {
            /* Write to FIFO */
            if (lora_mock.fifo_ptr < 255) {
                lora_mock.fifo[lora_mock.fifo_ptr++] = tx;
                lora_mock.fifo_len++;
            }
            return 0;
        }
        /* Read from FIFO */
        if (lora_mock.fifo_ptr < lora_mock.fifo_len) {
            return lora_mock.fifo[lora_mock.fifo_ptr++];
        }
        return 0;
    }

    if (is_write) {
        /* Write register */
        lora_mock.regs[reg] = tx;

        /* Simulate OpMode: mirror the mode bits back for read */
        if (reg == LORA_REG_OP_MODE) {
            /* When entering TX mode, mark TX active */
            if ((tx & 0x07) == LORA_MODE_TX) {
                lora_mock.tx_active = true;
                lora_mock.delay_calls = 0;
            }
            /* When entering RX single, prepare loopback RX */
            if ((tx & 0x07) == LORA_MODE_RX_SINGLE) {
                lora_mock.delay_calls = 0;
            }
        }

        /* When IRQ flags are written (cleared), reset them */
        if (reg == LORA_REG_IRQ_FLAGS) {
            /* writing 1 clears the flag per SX1276 spec */
            lora_mock.regs[reg] = (uint8_t)(lora_mock.regs[reg] & ~tx);
        }

        /* When FIFO addr ptr is written, update read position */
        if (reg == 0x0D) { /* REG_FIFO_ADDR_PTR */
            lora_mock.fifo_ptr = tx;
        }

        return 0;
    }

    /* Read register */
    return lora_mock.regs[reg];
}

static void lora_reset(void *ctx)
{
    (void)ctx;
    /* Hardware reset -- re-initialize chip ID */
    lora_mock.regs[LORA_REG_VERSION] = 0x12;
}

static void lora_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
    lora_mock.delay_calls++;

    /* After a few delay calls, simulate TX done or RX done */
    if (lora_mock.delay_calls >= 2) {
        if (lora_mock.tx_active) {
            lora_mock.regs[LORA_REG_IRQ_FLAGS] |= LORA_IRQ_TX_DONE;
            lora_mock.tx_active = false;
        }
        /* For RX: if we have FIFO data, signal RxDone */
        uint8_t mode = lora_mock.regs[LORA_REG_OP_MODE] & 0x07;
        if (mode == LORA_MODE_RX_SINGLE && lora_mock.fifo_len > 0) {
            lora_mock.regs[LORA_REG_IRQ_FLAGS] |= LORA_IRQ_RX_DONE;
            lora_mock.regs[0x1C] |= 0x40;  /* CrcOnPayload */
            lora_mock.regs[LORA_REG_RX_NB_BYTES] = lora_mock.fifo_len;
            lora_mock.regs[LORA_REG_FIFO_RX_CURRENT] = 0;
        }
    }
}

static const lora_spi_t lora_spi = {
    .cs_low   = lora_cs_low,
    .cs_high  = lora_cs_high,
    .xfer     = lora_xfer,
    .reset    = lora_reset,
    .delay_ms = lora_delay_ms,
    .ctx      = NULL,
};

/* ======================================================================
 * Global instances
 * ====================================================================== */

static nrf24_t nrf_dev;
static lora_t  lora_dev;

/* Simulated sensor data: temperature readings with repeated values
 * (good RLE compressibility) */
static const uint8_t sensor_payload[SENSOR_DATA_LEN] = {
    0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,  /* 8x 0x25 (37C) */
    0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,              /* 6x 0x1A (26C) */
    0x00, 0x00, 0x00, 0x00,                            /* 4x 0x00 (pad) */
    0x63, 0x63, 0x63, 0x63, 0x63,                      /* 5x 0x63 (99%) */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  /* 9x 0xFF   */
};

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. nRF24L01+ init */
    nrf_mock_reset();
    nrf24_err_t nerr = nrf24_init(&nrf_dev, &nrf_spi,
                                  NRF_CHANNEL, NRF_PAYLOAD_SIZE);
    ASSERT_EQ(nerr, NRF24_OK);

    /* Set TX/RX addresses */
    const uint8_t addr[5] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01 };
    nerr = nrf24_set_tx_addr(&nrf_dev, addr, 5);
    ASSERT_EQ(nerr, NRF24_OK);
    nerr = nrf24_set_rx_addr(&nrf_dev, 0, addr, 5);
    ASSERT_EQ(nerr, NRF24_OK);

    /* 2. LoRa SX1276 init */
    lora_mock_reset();
    lora_err_t lerr = lora_init(&lora_dev, &lora_spi, LORA_FREQ_HZ);
    ASSERT_EQ(lerr, LORA_OK);

    /* 3. SLIP, checksum, rle_codec are stateless -- no init needed.
     * Verify they return OK with trivial inputs. */
    uint8_t slip_out[16];
    uint16_t slip_len = sizeof(slip_out);
    slip_err_t serr = slip_encode(NULL, 0, slip_out, &slip_len);
    ASSERT_EQ(serr, SLIP_OK);

    uint8_t test_byte = 0xAA;
    uint8_t cs = checksum_xor(&test_byte, 1);
    ASSERT_EQ(cs, 0xAA);

    size_t rle_out_len = 0;
    uint8_t rle_out[4];
    rle_codec_err_t rerr = rle_codec_encode(&test_byte, 1,
                                            rle_out, sizeof(rle_out),
                                            &rle_out_len);
    ASSERT_EQ(rerr, RLE_CODEC_OK);
    ASSERT_EQ(rle_out_len, 2);

    printf("\n  All 6 wheels initialized successfully\n");
}

TEST(test_rle_compress_decompress)
{
    /* Compress sensor payload with RLE */
    uint8_t compressed[RLE_BUF_SIZE];
    size_t  comp_len = 0;

    rle_codec_err_t err = rle_codec_encode(sensor_payload, SENSOR_DATA_LEN,
                                           compressed, sizeof(compressed),
                                           &comp_len);
    ASSERT_EQ(err, RLE_CODEC_OK);
    ASSERT_TRUE(comp_len > 0);
    ASSERT_TRUE(comp_len < SENSOR_DATA_LEN); /* must compress */

    printf("\n  RLE: %u bytes -> %u bytes (%.0f%% ratio)\n",
           (unsigned)SENSOR_DATA_LEN, (unsigned)comp_len,
           (double)comp_len * 100.0 / (double)SENSOR_DATA_LEN);

    /* Decompress and verify exact match */
    uint8_t decompressed[SENSOR_DATA_LEN];
    size_t  decomp_len = 0;

    err = rle_codec_decode(compressed, comp_len,
                           decompressed, sizeof(decompressed),
                           &decomp_len);
    ASSERT_EQ(err, RLE_CODEC_OK);
    ASSERT_EQ(decomp_len, SENSOR_DATA_LEN);
    ASSERT_MEMEQ_N(decompressed, sensor_payload, SENSOR_DATA_LEN);
}

TEST(test_slip_frame_unframe)
{
    /* Frame the sensor payload with SLIP */
    uint8_t framed[SLIP_BUF_SIZE];
    uint16_t framed_len = sizeof(framed);

    slip_err_t err = slip_encode(sensor_payload, SENSOR_DATA_LEN,
                                 framed, &framed_len);
    ASSERT_EQ(err, SLIP_OK);
    ASSERT_TRUE(framed_len > SENSOR_DATA_LEN); /* framing adds overhead */

    /* Verify framing delimiters */
    ASSERT_EQ(framed[0], SLIP_END);
    ASSERT_EQ(framed[framed_len - 1], SLIP_END);

    /* Unframe and verify exact match */
    uint8_t unframed[SENSOR_DATA_LEN + 16];
    uint16_t unframed_len = sizeof(unframed);

    err = slip_decode(framed, framed_len, unframed, &unframed_len);
    ASSERT_EQ(err, SLIP_OK);
    ASSERT_EQ(unframed_len, SENSOR_DATA_LEN);
    ASSERT_MEMEQ_N(unframed, sensor_payload, SENSOR_DATA_LEN);

    printf("\n  SLIP: %u data bytes -> %u framed bytes\n",
           (unsigned)SENSOR_DATA_LEN, (unsigned)framed_len);
}

TEST(test_checksum_verify)
{
    /* Compute checksums over sensor payload */
    uint8_t  xor_cs  = checksum_xor(sensor_payload, SENSOR_DATA_LEN);
    uint8_t  sum_cs  = checksum_sum8(sensor_payload, SENSOR_DATA_LEN);
    uint16_t fletch  = checksum_fletcher16(sensor_payload, SENSOR_DATA_LEN);
    uint8_t  lrc_cs  = checksum_lrc(sensor_payload, SENSOR_DATA_LEN);

    /* Verify deterministic: same input -> same output */
    ASSERT_EQ(checksum_xor(sensor_payload, SENSOR_DATA_LEN), xor_cs);
    ASSERT_EQ(checksum_sum8(sensor_payload, SENSOR_DATA_LEN), sum_cs);
    ASSERT_EQ(checksum_fletcher16(sensor_payload, SENSOR_DATA_LEN), fletch);
    ASSERT_EQ(checksum_lrc(sensor_payload, SENSOR_DATA_LEN), lrc_cs);

    /* Modify one byte and verify checksum changes */
    uint8_t modified[SENSOR_DATA_LEN];
    memcpy(modified, sensor_payload, SENSOR_DATA_LEN);
    modified[0] ^= 0x01;  /* flip one bit */

    ASSERT_TRUE(checksum_xor(modified, SENSOR_DATA_LEN) != xor_cs);
    ASSERT_TRUE(checksum_fletcher16(modified, SENSOR_DATA_LEN) != fletch);

    printf("\n  Checksums: XOR=0x%02X SUM8=0x%02X Fletcher16=0x%04X LRC=0x%02X\n",
           xor_cs, sum_cs, fletch, lrc_cs);
}

TEST(test_nrf24_tx_rx_loopback)
{
    /* Reset mock for clean loopback test */
    nrf_mock_reset();
    nrf24_err_t err = nrf24_init(&nrf_dev, &nrf_spi,
                                 NRF_CHANNEL, NRF_PAYLOAD_SIZE);
    ASSERT_EQ(err, NRF24_OK);

    /* Prepare a short packet: RLE-compressed data + checksum */
    uint8_t compressed[RLE_BUF_SIZE];
    size_t comp_len = 0;
    rle_codec_err_t rerr = rle_codec_encode(sensor_payload, SENSOR_DATA_LEN,
                                            compressed, sizeof(compressed),
                                            &comp_len);
    ASSERT_EQ(rerr, RLE_CODEC_OK);
    ASSERT_TRUE(comp_len <= NRF_PAYLOAD_SIZE);

    /* Append XOR checksum */
    uint8_t tx_pkt[NRF_PAYLOAD_SIZE];
    memset(tx_pkt, 0, sizeof(tx_pkt));
    memcpy(tx_pkt, compressed, comp_len);
    tx_pkt[comp_len] = checksum_xor(compressed, comp_len);
    uint8_t pkt_len = (uint8_t)(comp_len + 1);

    /* Send via nRF24 */
    err = nrf24_send(&nrf_dev, tx_pkt, pkt_len);
    ASSERT_EQ(err, NRF24_OK);

    /* Switch to RX mode and read loopback */
    err = nrf24_set_mode(&nrf_dev, NRF24_MODE_RX);
    ASSERT_EQ(err, NRF24_OK);

    ASSERT_TRUE(nrf24_data_ready(&nrf_dev));

    uint8_t rx_buf[NRF_PAYLOAD_SIZE];
    uint8_t rx_len = 0;
    err = nrf24_receive(&nrf_dev, rx_buf, NRF_PAYLOAD_SIZE, &rx_len);
    ASSERT_EQ(err, NRF24_OK);
    ASSERT_EQ(rx_len, NRF_PAYLOAD_SIZE);

    /* Verify the received compressed data + checksum */
    ASSERT_MEMEQ_N(rx_buf, tx_pkt, pkt_len);

    /* Verify checksum on received data */
    uint8_t rx_cs = checksum_xor(rx_buf, comp_len);
    ASSERT_EQ(rx_cs, rx_buf[comp_len]);

    printf("\n  nRF24 loopback: sent %u bytes, received %u, checksum OK\n",
           (unsigned)pkt_len, (unsigned)rx_len);
}

TEST(test_lora_send_receive)
{
    /* Reset mock for clean loopback */
    lora_mock_reset();
    lora_err_t err = lora_init(&lora_dev, &lora_spi, LORA_FREQ_HZ);
    ASSERT_EQ(err, LORA_OK);

    /* SLIP-frame the sensor data for LoRa transmission */
    uint8_t framed[SLIP_BUF_SIZE];
    uint16_t framed_len = sizeof(framed);
    slip_err_t serr = slip_encode(sensor_payload, SENSOR_DATA_LEN,
                                  framed, &framed_len);
    ASSERT_EQ(serr, SLIP_OK);

    /* Add Fletcher-16 checksum to the framed data */
    uint16_t fletch = checksum_fletcher16(framed, framed_len);
    uint8_t tx_pkt[SLIP_BUF_SIZE + 2];
    memcpy(tx_pkt, framed, framed_len);
    tx_pkt[framed_len]     = (uint8_t)(fletch >> 8);
    tx_pkt[framed_len + 1] = (uint8_t)(fletch & 0xFF);
    uint8_t tx_total = (uint8_t)(framed_len + 2);

    /* Send via LoRa */
    err = lora_send(&lora_dev, tx_pkt, tx_total);
    ASSERT_EQ(err, LORA_OK);

    /* The mock FIFO still has the data; prepare for RX by resetting
     * FIFO pointer and keeping fifo_len intact */
    lora_mock.fifo_ptr = 0;
    lora_mock.regs[LORA_REG_OP_MODE] = LORA_OPMODE_LORA; /* standby */
    lora_mock.delay_calls = 0;

    /* Receive */
    uint8_t rx_buf[RADIO_BUF_SIZE];
    uint8_t rx_len = 0;
    err = lora_receive(&lora_dev, rx_buf, &rx_len, 255);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(rx_len, tx_total);

    /* Verify Fletcher-16 on received framed data */
    uint16_t rx_fletch = (uint16_t)((uint16_t)rx_buf[rx_len - 2] << 8
                                    | rx_buf[rx_len - 1]);
    uint16_t calc_fletch = checksum_fletcher16(rx_buf,
                                               (size_t)(rx_len - 2));
    ASSERT_EQ(rx_fletch, calc_fletch);

    /* SLIP-decode the received framed data */
    uint8_t decoded[SENSOR_DATA_LEN + 16];
    uint16_t decoded_len = sizeof(decoded);
    serr = slip_decode(rx_buf, (uint16_t)(rx_len - 2),
                       decoded, &decoded_len);
    ASSERT_EQ(serr, SLIP_OK);
    ASSERT_EQ(decoded_len, SENSOR_DATA_LEN);
    ASSERT_MEMEQ_N(decoded, sensor_payload, SENSOR_DATA_LEN);

    printf("\n  LoRa loopback: sent %u bytes, received %u, checksum OK\n",
           (unsigned)tx_total, (unsigned)rx_len);
}

TEST(test_lorawan_packet_build)
{
    /* Build a LoRaWAN unconfirmed uplink with sensor data */
    uint8_t mic[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

    lorawan_frame_t frame = {
        .mtype       = LORAWAN_UNCONFIRMED_UP,
        .dev_addr    = 0x26011234,
        .fcnt        = 42,
        .fport       = 1,
        .ack         = false,
        .adr         = true,
        .payload     = sensor_payload,
        .payload_len = SENSOR_DATA_LEN,
        .mic         = { 0 },
    };
    memcpy(frame.mic, mic, 4);

    uint8_t pkt_buf[128];
    int built = lorawan_build_data(pkt_buf, sizeof(pkt_buf), &frame);
    ASSERT_TRUE(built > 0);

    /* Expected: MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + FPort(1)
     *           + Payload(32) + MIC(4) = 45 */
    ASSERT_EQ(built, 45);

    /* Verify MHDR: MType=UNCONFIRMED_UP (0x02 << 5 = 0x40) */
    ASSERT_EQ_HEX(pkt_buf[0], 0x40);

    /* Parse it back */
    lorawan_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    lorawan_err_t perr = lorawan_parse_data(pkt_buf, (uint16_t)built,
                                            &parsed);
    ASSERT_EQ(perr, LORAWAN_OK);
    ASSERT_EQ(parsed.mtype, LORAWAN_UNCONFIRMED_UP);
    ASSERT_EQ(parsed.dev_addr, 0x26011234);
    ASSERT_EQ(parsed.fcnt, 42);
    ASSERT_EQ(parsed.fport, 1);
    ASSERT_TRUE(parsed.adr);
    ASSERT_FALSE(parsed.ack);
    ASSERT_EQ(parsed.payload_len, SENSOR_DATA_LEN);
    ASSERT_MEMEQ_N(parsed.mic, mic, 4);

    printf("\n  LoRaWAN uplink: %d bytes, DevAddr=0x%08X, FCnt=%u\n",
           built, (unsigned)parsed.dev_addr, (unsigned)parsed.fcnt);
}

TEST(test_full_mesh_cycle)
{
    /*
     * Full mesh cycle:
     *   1. Compress sensor data with RLE
     *   2. Add checksum to compressed data
     *   3. Frame with SLIP for serial transport
     *   4. Transmit via nRF24 (short range, inter-node)
     *   5. Receive and strip SLIP framing
     *   6. Verify checksum
     *   7. Decompress RLE
     *   8. Build LoRaWAN uplink for gateway
     *   9. Verify full round-trip integrity
     */

    printf("\n  --- Full Mesh Cycle ---\n");

    /* Step 1: RLE compress */
    uint8_t compressed[RLE_BUF_SIZE];
    size_t comp_len = 0;
    rle_codec_err_t rerr = rle_codec_encode(sensor_payload, SENSOR_DATA_LEN,
                                            compressed, sizeof(compressed),
                                            &comp_len);
    ASSERT_EQ(rerr, RLE_CODEC_OK);
    printf("  1. RLE compress: %u -> %u bytes\n",
           (unsigned)SENSOR_DATA_LEN, (unsigned)comp_len);

    /* Step 2: Append Fletcher-16 checksum */
    uint8_t with_cs[RLE_BUF_SIZE + 2];
    memcpy(with_cs, compressed, comp_len);
    uint16_t fletch = checksum_fletcher16(compressed, comp_len);
    with_cs[comp_len]     = (uint8_t)(fletch >> 8);
    with_cs[comp_len + 1] = (uint8_t)(fletch & 0xFF);
    size_t cs_total = comp_len + 2;
    printf("  2. Checksum: Fletcher16=0x%04X, total %u bytes\n",
           fletch, (unsigned)cs_total);

    /* Step 3: SLIP frame */
    uint8_t framed[SLIP_BUF_SIZE];
    uint16_t framed_len = sizeof(framed);
    slip_err_t serr = slip_encode(with_cs, (uint16_t)cs_total,
                                  framed, &framed_len);
    ASSERT_EQ(serr, SLIP_OK);
    printf("  3. SLIP frame: %u -> %u bytes\n",
           (unsigned)cs_total, (unsigned)framed_len);

    /* Step 4: Transmit via nRF24 (use first NRF_PAYLOAD_SIZE bytes,
     * simulating a mesh hop where the relay forwards small packets) */
    nrf_mock_reset();
    nrf24_err_t nerr = nrf24_init(&nrf_dev, &nrf_spi,
                                  NRF_CHANNEL, NRF_PAYLOAD_SIZE);
    ASSERT_EQ(nerr, NRF24_OK);

    /* Send the framed data (fits within payload size for this test) */
    ASSERT_TRUE(framed_len <= NRF_PAYLOAD_SIZE);
    nerr = nrf24_send(&nrf_dev, framed, (uint8_t)framed_len);
    ASSERT_EQ(nerr, NRF24_OK);
    printf("  4. nRF24 TX: %u bytes sent\n", (unsigned)framed_len);

    /* Step 5: Receive via nRF24 and SLIP decode */
    nerr = nrf24_set_mode(&nrf_dev, NRF24_MODE_RX);
    ASSERT_EQ(nerr, NRF24_OK);

    uint8_t rx_buf[NRF_PAYLOAD_SIZE];
    uint8_t rx_len = 0;
    nerr = nrf24_receive(&nrf_dev, rx_buf, NRF_PAYLOAD_SIZE, &rx_len);
    ASSERT_EQ(nerr, NRF24_OK);
    printf("  5. nRF24 RX: %u bytes received\n", (unsigned)rx_len);

    /* SLIP decode the received data */
    uint8_t unframed[RLE_BUF_SIZE + 2];
    uint16_t unframed_len = sizeof(unframed);
    serr = slip_decode(rx_buf, rx_len, unframed, &unframed_len);
    ASSERT_EQ(serr, SLIP_OK);
    ASSERT_EQ(unframed_len, (uint16_t)cs_total);
    printf("     SLIP decode: %u -> %u bytes\n",
           (unsigned)rx_len, (unsigned)unframed_len);

    /* Step 6: Verify checksum */
    uint16_t rx_fletch = (uint16_t)((uint16_t)unframed[unframed_len - 2] << 8
                                    | unframed[unframed_len - 1]);
    uint16_t calc_cs = checksum_fletcher16(unframed,
                                           (size_t)(unframed_len - 2));
    ASSERT_EQ(rx_fletch, calc_cs);
    printf("  6. Checksum verified: 0x%04X\n", rx_fletch);

    /* Step 7: RLE decompress (strip checksum bytes first) */
    uint8_t decompressed[SENSOR_DATA_LEN];
    size_t decomp_len = 0;
    rerr = rle_codec_decode(unframed, (size_t)(unframed_len - 2),
                            decompressed, sizeof(decompressed),
                            &decomp_len);
    ASSERT_EQ(rerr, RLE_CODEC_OK);
    ASSERT_EQ(decomp_len, SENSOR_DATA_LEN);
    ASSERT_MEMEQ_N(decompressed, sensor_payload, SENSOR_DATA_LEN);
    printf("  7. RLE decompress: %u -> %u bytes, data matches original\n",
           (unsigned)(unframed_len - 2), (unsigned)decomp_len);

    /* Step 8: Build LoRaWAN uplink packet from decompressed data */
    uint8_t mic[4] = { 0x11, 0x22, 0x33, 0x44 };
    lorawan_frame_t uplink = {
        .mtype       = LORAWAN_UNCONFIRMED_UP,
        .dev_addr    = 0x26014567,
        .fcnt        = 1,
        .fport       = 2,
        .ack         = false,
        .adr         = true,
        .payload     = decompressed,
        .payload_len = (uint16_t)decomp_len,
        .mic         = { 0 },
    };
    memcpy(uplink.mic, mic, 4);

    uint8_t lorawan_buf[128];
    int lorawan_len = lorawan_build_data(lorawan_buf, sizeof(lorawan_buf),
                                         &uplink);
    ASSERT_TRUE(lorawan_len > 0);
    printf("  8. LoRaWAN uplink: %d bytes built\n", lorawan_len);

    /* Step 9: Parse back and verify end-to-end integrity */
    lorawan_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    lorawan_err_t lerr = lorawan_parse_data(lorawan_buf,
                                            (uint16_t)lorawan_len,
                                            &parsed);
    ASSERT_EQ(lerr, LORAWAN_OK);
    ASSERT_EQ(parsed.dev_addr, 0x26014567);
    ASSERT_EQ(parsed.payload_len, SENSOR_DATA_LEN);
    ASSERT_NOT_NULL(parsed.payload);
    ASSERT_MEMEQ_N(parsed.payload, sensor_payload, SENSOR_DATA_LEN);
    printf("  9. End-to-end verified: payload matches original sensor data\n");
    printf("  --- Mesh cycle complete ---\n");
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("wireless_mesh integration")
    RUN(test_init_all);
    RUN(test_rle_compress_decompress);
    RUN(test_slip_frame_unframe);
    RUN(test_checksum_verify);
    RUN(test_nrf24_tx_rx_loopback);
    RUN(test_lora_send_receive);
    RUN(test_lorawan_packet_build);
    RUN(test_full_mesh_cycle);
TEST_MAIN_END()
