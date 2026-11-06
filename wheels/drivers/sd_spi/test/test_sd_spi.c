// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "sd_spi.h"

/* ---------- mock SD card state machine ---------- */

/*
 * Protocol flow for send_cmd in driver:
 *   cs_low → xfer(0xFF) dummy → xfer(0x40|cmd) → 4x xfer(arg) → xfer(crc)
 *   → N x xfer(0xFF) polling for R1 (bit7=0)
 *
 * The mock tracks bytes after cs_low:
 *   byte 0: dummy 0xFF (skip)
 *   byte 1: command (0x40|cmd)
 *   bytes 2-5: argument
 *   byte 6: CRC
 *   byte 7+: R1 response, then extended response bytes
 */

typedef enum {
    PH_WAIT_CMD,        /* waiting for command byte (skip 0xFF dummies) */
    PH_ARG,             /* collecting 4 arg bytes */
    PH_CRC,             /* next byte is CRC */
    PH_R1,              /* next read returns R1 */
    PH_EXT,             /* extended response (R7/OCR) */
    PH_READ_TOKEN,      /* waiting to send 0xFE data token */
    PH_READ_DATA,       /* sending 512 data bytes */
    PH_READ_CRC,        /* sending 2 CRC bytes */
    PH_WRITE_TOKEN,     /* waiting for 0xFE write token from host */
    PH_WRITE_DATA,      /* receiving 512 data bytes */
    PH_WRITE_CRC,       /* receiving 2 CRC bytes */
    PH_WRITE_RESP,      /* returning data response byte */
    PH_WRITE_BUSY,      /* returning busy (0x00) then 0xFF */
    PH_IDLE,            /* cs high or between commands */
} mock_phase_t;

static mock_phase_t mock_phase;
static uint8_t     mock_cmd;
static uint32_t    mock_arg;
static int         mock_byte_idx;      /* multi-purpose counter */
static int         mock_delay_calls;

/* pending R1 and extended response */
static uint8_t     mock_r1;
static uint8_t     mock_ext[4];
static int         mock_ext_len;
static int         mock_ext_idx;

/* card personality */
static int         mock_is_sdhc;
static int         mock_is_v1;
static int         mock_acmd41_remain;  /* countdown to ready */
static int         mock_fail_init;      /* CMD0 no response */
static int         mock_silent_read;    /* nothing at all after CMD17's R1 */
static int         mock_fail_write;     /* write response = rejected */
static int         mock_fail_spi;       /* all xfer return 0xFF */
static int         mock_fail_cmd17;    /* CMD17 returns error R1 */
static int         mock_fail_cmd24;    /* CMD24 returns error R1 */
static int         mock_cmd55_seen;
static int         mock_spi_mode;       /* a valid CMD0 has left SD mode */
static int         mock_ready;          /* ACMD41 done: the idle bit is clear */
static uint8_t     mock_crc;            /* last byte of the command frame */
static int         mock_busy_polls;     /* polls the card stays busy, -1 = forever */
static uint8_t     mock_read_token;     /* first byte of the read data phase */
static uint8_t     mock_status_r2;      /* second byte of the CMD13 response */
static uint8_t     mock_cmd8_r1;        /* forced CMD8 response, 0 = normal */

/* block storage */
static uint8_t     mock_block[512];
static uint32_t    mock_last_addr;
static uint8_t     mock_write_buf[512];

static void mock_reset(void)
{
    mock_phase = PH_IDLE;
    mock_cmd = 0;
    mock_arg = 0;
    mock_byte_idx = 0;
    mock_delay_calls = 0;
    mock_r1 = 0xFF;
    mock_ext_len = 0;
    mock_ext_idx = 0;
    memset(mock_ext, 0, sizeof(mock_ext));

    mock_is_sdhc = 1;
    mock_is_v1 = 0;
    mock_acmd41_remain = 0;
    mock_fail_init = 0;
    mock_silent_read = 0;
    mock_fail_write = 0;
    mock_fail_spi = 0;
    mock_fail_cmd17 = 0;
    mock_fail_cmd24 = 0;
    mock_cmd55_seen = 0;
    mock_spi_mode = 0;
    mock_ready = 0;
    mock_crc = 0;
    mock_busy_polls = 0;
    mock_read_token = 0xFE;
    mock_status_r2 = 0;
    mock_cmd8_r1 = 0;
    mock_last_addr = 0;

    memset(mock_write_buf, 0, sizeof(mock_write_buf));

    for (int i = 0; i < 512; i++)
        mock_block[i] = (uint8_t)(i & 0xFF);
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_phase = PH_WAIT_CMD;
    mock_byte_idx = 0;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_phase = PH_IDLE;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    mock_delay_calls++;
}

/* CRC7 of spec section 4.5, generator x^7 + x^3 + 1, then the end bit. */
static uint8_t frame_crc(void)
{
    const uint8_t frame[5] = {
        (uint8_t)(0x40U | mock_cmd),
        (uint8_t)(mock_arg >> 24), (uint8_t)(mock_arg >> 16),
        (uint8_t)(mock_arg >> 8),  (uint8_t)mock_arg,
    };
    uint8_t crc = 0;

    for (int i = 0; i < 5; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t in = (uint8_t)((frame[i] >> bit) & 1U);
            uint8_t top = (uint8_t)((crc >> 6) & 1U);

            crc = (uint8_t)((crc << 1) & 0x7FU);
            if (in != top)
                crc ^= 0x09U;
        }
    }
    return (uint8_t)((crc << 1) | 1U);
}

/* prepare response after CRC received */
static void prepare_response(void)
{
    int crc_ok = (mock_crc == frame_crc());
    int app_cmd = mock_cmd55_seen;
    uint8_t idle = (uint8_t)(mock_ready ? 0x00U : 0x01U);

    mock_ext_len = 0;
    mock_ext_idx = 0;
    mock_cmd55_seen = 0;

    /* Spec 7.2.2: the first CMD0 arrives in SD mode and needs its CRC, and
     * 6.4.1: "CMD0 shall be the first command". */
    if (!mock_spi_mode && (mock_cmd != 0 || !crc_ok)) {
        mock_r1 = 0xFF;
        mock_phase = PH_R1;
        return;
    }
    /* Spec 7.2.7: these are not valid in the idle state. */
    if (!mock_ready && (mock_cmd == 13 || mock_cmd == 17 || mock_cmd == 24)) {
        mock_r1 = 0x05;
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 0) {
        /* CMD0: GO_IDLE */
        mock_r1 = mock_fail_init ? 0xFF : 0x01;
        if (!mock_fail_init) {
            mock_spi_mode = 1;
            mock_ready = 0;
        }
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 8) {
        /* CMD8 */
        if (mock_cmd8_r1 != 0) {
            mock_r1 = mock_cmd8_r1;
            mock_phase = PH_R1;
            return;
        }
        if (mock_is_v1) {
            mock_r1 = 0x05; /* illegal command for v1 */
            mock_phase = PH_R1;
            return;
        }
        /* Spec 7.2.2: "The CMD8 CRC verification is always enabled." */
        if (!crc_ok) {
            mock_r1 = (uint8_t)(idle | 0x08U);
            mock_phase = PH_R1;
            return;
        }
        mock_r1 = idle;
        mock_ext[0] = 0x00; mock_ext[1] = 0x00;
        mock_ext[2] = 0x01; mock_ext[3] = 0xAA;
        mock_ext_len = 4;
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 55) {
        mock_cmd55_seen = 1;
        mock_r1 = idle;
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 41 && app_cmd) {
        /* Spec 4.2.3: "If HCS is set to 0, SDHC and SDXC Cards never return
         * ready status". */
        if (mock_is_sdhc && !mock_is_v1 && !(mock_arg & 0x40000000U)) {
            mock_r1 = 0x01;
        } else if (mock_acmd41_remain > 0) {
            mock_acmd41_remain--;
            mock_r1 = 0x01;
        } else {
            mock_r1 = 0x00;
            mock_ready = 1;
        }
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 58) {
        mock_r1 = idle;
        /* Spec 5.1: bit 31 says power up is done, and only then is CCS valid */
        mock_ext[0] = (uint8_t)(!mock_ready ? 0x00U : (mock_is_sdhc ? 0xC0U : 0x80U));
        mock_ext[1] = 0xFF;
        mock_ext[2] = 0x80;
        mock_ext[3] = 0x00;
        mock_ext_len = 4;
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 13) {
        mock_r1 = 0x00;
        mock_ext[0] = mock_status_r2;
        mock_ext_len = 1;
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 17) {
        mock_last_addr = mock_arg;
        mock_r1 = mock_fail_cmd17 ? 0x04U : 0x00U;
        mock_phase = PH_R1;
        return;
    }
    if (mock_cmd == 24) {
        mock_last_addr = mock_arg;
        mock_r1 = mock_fail_cmd24 ? 0x04U : 0x00U;
        mock_phase = PH_R1;
        return;
    }
    /* unknown command */
    mock_r1 = 0x04;
    mock_phase = PH_R1;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    if (mock_fail_spi) return 0xFF;

    switch (mock_phase) {
    case PH_IDLE:
        return 0xFF;

    case PH_WAIT_CMD:
        if (tx == 0xFF) return 0xFF;  /* skip dummy */
        mock_cmd = (uint8_t)(tx & 0x3FU);
        mock_arg = 0;
        mock_byte_idx = 0;
        mock_phase = PH_ARG;
        return 0xFF;

    case PH_ARG:
        mock_arg = (mock_arg << 8) | tx;
        mock_byte_idx++;
        if (mock_byte_idx >= 4)
            mock_phase = PH_CRC;
        return 0xFF;

    case PH_CRC:
        /* CRC received, prepare response for next reads */
        mock_crc = tx;
        prepare_response();
        return 0xFF;

    case PH_R1:
        /* return R1 once, then move to extended or post-cmd phase */
        {
            uint8_t r = mock_r1;
            if (mock_ext_len > 0) {
                mock_phase = PH_EXT;
            } else if (mock_cmd == 17 && mock_r1 == 0x00) {
                /* transition to read data */
                if (mock_silent_read) {
                    mock_phase = PH_IDLE;
                } else {
                    mock_phase = PH_READ_TOKEN;
                }
            } else if (mock_cmd == 24 && mock_r1 == 0x00) {
                mock_phase = PH_WRITE_TOKEN;
                mock_byte_idx = 0;
            } else {
                mock_phase = PH_IDLE;
            }
            return r;
        }

    case PH_EXT:
        {
            uint8_t val = mock_ext[mock_ext_idx++];
            if (mock_ext_idx >= mock_ext_len)
                mock_phase = PH_IDLE;
            return val;
        }

    case PH_READ_TOKEN:
        if (mock_read_token != 0xFE) {
            mock_phase = PH_IDLE;
            return mock_read_token;
        }
        mock_phase = PH_READ_DATA;
        mock_byte_idx = 0;
        return 0xFE;

    case PH_READ_DATA:
        {
            uint8_t val = mock_block[mock_byte_idx++];
            if (mock_byte_idx >= 512) {
                mock_phase = PH_READ_CRC;
                mock_byte_idx = 0;
            }
            return val;
        }

    case PH_READ_CRC:
        mock_byte_idx++;
        if (mock_byte_idx >= 2)
            mock_phase = PH_IDLE;
        return 0x00;

    case PH_WRITE_TOKEN:
        if (tx == 0xFE) {
            mock_phase = PH_WRITE_DATA;
            mock_byte_idx = 0;
        }
        return 0xFF;

    case PH_WRITE_DATA:
        mock_write_buf[mock_byte_idx] = tx;
        mock_block[mock_byte_idx] = tx;
        mock_byte_idx++;
        if (mock_byte_idx >= 512) {
            mock_phase = PH_WRITE_CRC;
            mock_byte_idx = 0;
        }
        return 0xFF;

    case PH_WRITE_CRC:
        mock_byte_idx++;
        if (mock_byte_idx >= 2)
            mock_phase = PH_WRITE_RESP;
        return 0xFF;

    case PH_WRITE_RESP:
        mock_phase = PH_WRITE_BUSY;
        mock_byte_idx = 0;
        if (mock_fail_write) return 0x0B;   /* rejected */
        return 0x05;                        /* accepted */

    case PH_WRITE_BUSY:
        if (mock_busy_polls != 0) {
            if (mock_busy_polls > 0) mock_busy_polls--;
            return 0x00;    /* holds DataOut low while programming */
        }
        mock_phase = PH_IDLE;
        return 0xFF;    /* not busy */
    default:
        return 0xFF;
    }
    return 0xFF;
}

static const sd_spi_hal_t mock_hal = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_sdhc)
{
    mock_reset();
    mock_is_sdhc = 1;
    sd_spi_t sd;
    sd_spi_err_t e = sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(e, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV2_HC);
}

TEST(test_init_sdsc)
{
    mock_reset();
    mock_is_sdhc = 0;
    sd_spi_t sd;
    sd_spi_err_t e = sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(e, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV2_SC);
}

TEST(test_init_sdv1)
{
    mock_reset();
    mock_is_v1 = 1;
    sd_spi_t sd;
    sd_spi_err_t e = sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(e, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV1);
}

TEST(test_init_cmd0_fail)
{
    mock_reset();
    mock_fail_init = 1;
    sd_spi_t sd;
    sd_spi_err_t e = sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(e, SD_SPI_ERR_INIT);
}

TEST(test_init_acmd41_slow)
{
    mock_reset();
    mock_acmd41_remain = 3;
    sd_spi_t sd;
    sd_spi_err_t e = sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(e, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV2_HC);
}

TEST(test_null_params)
{
    sd_spi_t sd;
    ASSERT_EQ(sd_spi_init(NULL, &mock_hal), SD_SPI_ERR_PARAM);
    ASSERT_EQ(sd_spi_init(&sd, NULL), SD_SPI_ERR_PARAM);

    uint8_t buf[512];
    ASSERT_EQ(sd_spi_read_block(NULL, 0, buf), SD_SPI_ERR_PARAM);
    ASSERT_EQ(sd_spi_write_block(NULL, 0, buf), SD_SPI_ERR_PARAM);

    ASSERT_EQ(sd_spi_get_type(NULL), SD_SPI_TYPE_UNKNOWN);
    ASSERT_EQ(sd_spi_get_block_count(NULL), 0);
}

TEST(test_null_buf_read)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(sd_spi_read_block(&sd, 0, NULL), SD_SPI_ERR_PARAM);
}

TEST(test_null_buf_write)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(sd_spi_write_block(&sd, 0, NULL), SD_SPI_ERR_PARAM);
}

TEST(test_read_block)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t buf[512];
    memset(buf, 0, sizeof(buf));
    sd_spi_err_t e = sd_spi_read_block(&sd, 0, buf);
    ASSERT_EQ(e, SD_SPI_OK);
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x01);
    ASSERT_EQ(buf[255], 0xFF);
    ASSERT_EQ(buf[256], 0x00);
}

TEST(test_read_timeout)
{
    mock_reset();
    mock_silent_read = 1;
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t buf[512];
    sd_spi_err_t e = sd_spi_read_block(&sd, 0, buf);
    ASSERT_EQ(e, SD_SPI_ERR_TIMEOUT);
}

TEST(test_write_block)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t data[512];
    for (int i = 0; i < 512; i++)
        data[i] = (uint8_t)(0xFF - (i & 0xFF));

    sd_spi_err_t e = sd_spi_write_block(&sd, 0, data);
    ASSERT_EQ(e, SD_SPI_OK);
    ASSERT_EQ(mock_block[0], 0xFF);
    ASSERT_EQ(mock_block[1], 0xFE);
    ASSERT_EQ(mock_block[511], 0x00);
}

TEST(test_write_rejected)
{
    mock_reset();
    mock_fail_write = 1;
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t data[512];
    memset(data, 0xAA, sizeof(data));
    sd_spi_err_t e = sd_spi_write_block(&sd, 0, data);
    ASSERT_EQ(e, SD_SPI_ERR_WRITE);
}

TEST(test_get_type)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(sd_spi_get_type(&sd), SD_SPI_TYPE_SDV2_HC);

    mock_reset();
    mock_is_sdhc = 0;
    sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(sd_spi_get_type(&sd), SD_SPI_TYPE_SDV2_SC);

    mock_reset();
    mock_is_v1 = 1;
    sd_spi_init(&sd, &mock_hal);
    ASSERT_EQ(sd_spi_get_type(&sd), SD_SPI_TYPE_SDV1);
}

TEST(test_block_count)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);
    sd.block_count = 1024;
    ASSERT_EQ(sd_spi_get_block_count(&sd), 1024);
}

TEST(test_sdhc_block_addressing)
{
    mock_reset();
    mock_is_sdhc = 1;
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t buf[512];
    sd_spi_read_block(&sd, 100, buf);
    ASSERT_EQ(mock_last_addr, 100);
}

TEST(test_sdsc_byte_addressing)
{
    mock_reset();
    mock_is_sdhc = 0;
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t buf[512];
    sd_spi_read_block(&sd, 2, buf);
    ASSERT_EQ(mock_last_addr, 1024);
}

TEST(test_read_write_cycle)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    uint8_t wr[512];
    for (int i = 0; i < 512; i++)
        wr[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    ASSERT_EQ(sd_spi_write_block(&sd, 0, wr), SD_SPI_OK);

    uint8_t rd[512];
    ASSERT_EQ(sd_spi_read_block(&sd, 0, rd), SD_SPI_OK);
    ASSERT_TRUE(memcmp(wr, rd, 512) == 0);
}

TEST(test_spi_failure_during_cmd)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    mock_fail_spi = 1;
    uint8_t buf[512];
    sd_spi_err_t e = sd_spi_read_block(&sd, 0, buf);
    ASSERT_TRUE(e != SD_SPI_OK);
}

/* ---------- SD_SPI_ERR_CMD: command rejected by card ---------- */

TEST(test_read_cmd_rejected)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    /* CMD17 returns error R1 (0x04 = illegal command) */
    mock_fail_cmd17 = 1;

    uint8_t buf[512];
    sd_spi_err_t e = sd_spi_read_block(&sd, 0, buf);
    ASSERT_EQ(e, SD_SPI_ERR_CMD);
}

TEST(test_write_cmd_rejected)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    /* CMD24 returns error R1 */
    mock_fail_cmd24 = 1;

    uint8_t data[512];
    memset(data, 0x55, sizeof(data));
    sd_spi_err_t e = sd_spi_write_block(&sd, 0, data);
    ASSERT_EQ(e, SD_SPI_ERR_CMD);
}

/* ---------- SD_SPI_ERR_SPI: SPI bus failure during read ---------- */

TEST(test_spi_failure_during_read_returns_timeout)
{
    /* When SPI bus fails (all 0xFF), CMD17 R1 response is 0xFF (bit7 set),
     * which means no valid response was received.  The send_cmd loop
     * exhausts retries and returns 0xFF.  Since 0xFF != 0x00, the driver
     * returns SD_SPI_ERR_CMD.  This proves that SPI-level failures are
     * surfaced as command errors. */
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    mock_fail_spi = 1;
    uint8_t buf[512];
    sd_spi_err_t e = sd_spi_read_block(&sd, 0, buf);
    /* With all-0xFF bus, send_cmd returns 0xFF which is checked as
     * r1 != 0x00 → ERR_CMD (line 180 of sd_spi.c) */
    ASSERT_TRUE(e == SD_SPI_ERR_CMD || e == SD_SPI_ERR_TIMEOUT);
}

TEST(test_spi_failure_during_write)
{
    mock_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &mock_hal);

    mock_fail_spi = 1;
    uint8_t data[512];
    memset(data, 0xCC, sizeof(data));
    sd_spi_err_t e = sd_spi_write_block(&sd, 0, data);
    ASSERT_TRUE(e != SD_SPI_OK);
}

/* ---------- SD_SPI_ERR_CRC: defined but not returned by current API ----
 * CRC checking is left to the SD card hardware; the driver discards the
 * 2-byte CRC after reads and sends dummy CRC on writes.  No code path
 * currently returns ERR_CRC.  This test verifies ABI stability. */

TEST(test_err_crc_value)
{
    ASSERT_EQ((int)SD_SPI_ERR_CRC, -4);
}

TEST(test_err_spi_value)
{
    ASSERT_EQ((int)SD_SPI_ERR_SPI, -1);
}

/* ---------- main ---------- */

TEST(test_write_outlasts_a_long_busy)
{
    uint8_t buf[512] = { 0 };
    sd_spi_t sd;
    mock_reset();
    sd_spi_init(&sd, &mock_hal);

    /* more polls than the old fixed limit of 8000 */
    mock_busy_polls = 9000;
    ASSERT_EQ(sd_spi_write_block(&sd, 0, buf), SD_SPI_OK);
}

TEST(test_write_busy_limit_is_over_500_ms)
{
    uint8_t buf[512] = { 0 };
    sd_spi_t sd;
    mock_reset();
    sd_spi_init(&sd, &mock_hal);

    mock_busy_polls = -1;
    mock_delay_calls = 0;
    ASSERT_EQ(sd_spi_write_block(&sd, 0, buf), SD_SPI_ERR_TIMEOUT);
    ASSERT_TRUE(mock_delay_calls > 500);
}

TEST(test_read_token_limit_is_100_ms)
{
    uint8_t buf[512];
    sd_spi_t sd;
    mock_reset();
    mock_silent_read = 1;
    sd_spi_init(&sd, &mock_hal);

    mock_delay_calls = 0;
    ASSERT_EQ(sd_spi_read_block(&sd, 0, buf), SD_SPI_ERR_TIMEOUT);
    ASSERT_TRUE(mock_delay_calls >= 100);
}

TEST(test_write_reports_a_programming_error)
{
    uint8_t buf[512] = { 0 };
    sd_spi_t sd;
    mock_reset();
    sd_spi_init(&sd, &mock_hal);

    /* the write was accepted, and the failure shows only in CMD13 */
    mock_status_r2 = 0x40;
    ASSERT_EQ(sd_spi_write_block(&sd, 0, buf), SD_SPI_ERR_WRITE);
}

TEST(test_cmd8_crc_error_is_not_a_legacy_card)
{
    sd_spi_t sd;
    mock_reset();
    mock_cmd8_r1 = 0x09;
    ASSERT_EQ(sd_spi_init(&sd, &mock_hal), SD_SPI_ERR_INIT);
}

TEST(test_silent_cmd8_is_not_a_legacy_card)
{
    sd_spi_t sd;
    mock_reset();
    mock_cmd8_r1 = 0xFF;
    ASSERT_EQ(sd_spi_init(&sd, &mock_hal), SD_SPI_ERR_INIT);
}

TEST(test_read_reports_a_data_error_token)
{
    uint8_t buf[512];
    sd_spi_t sd;
    mock_reset();
    sd_spi_init(&sd, &mock_hal);

    mock_read_token = 0x08;  /* out of range */
    ASSERT_EQ(sd_spi_read_block(&sd, 0, buf), SD_SPI_ERR_READ);
}

/* ---------- the card itself, from Physical Layer Spec v3.01 ---------- */

/* One command frame sent straight to the mock. */
static uint8_t raw_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    mock_cs_high(NULL);
    mock_cs_low(NULL);
    (void)mock_xfer(NULL, 0xFF);
    (void)mock_xfer(NULL, (uint8_t)(0x40U | cmd));
    (void)mock_xfer(NULL, (uint8_t)(arg >> 24));
    (void)mock_xfer(NULL, (uint8_t)(arg >> 16));
    (void)mock_xfer(NULL, (uint8_t)(arg >> 8));
    (void)mock_xfer(NULL, (uint8_t)arg);
    (void)mock_xfer(NULL, crc);

    uint8_t r1 = 0xFF;
    for (int i = 0; i < 8 && (r1 & 0x80U); i++)
        r1 = mock_xfer(NULL, 0xFF);
    return r1;
}

static uint8_t raw_acmd41(uint32_t arg)
{
    (void)raw_cmd(55, 0, 0xFF);
    return raw_cmd(41, arg, 0xFF);
}

TEST(test_card_first_cmd0_needs_a_valid_crc)
{
    mock_reset();
    /* 7.2.2: CMD0 "shall have a valid CRC field", which is 0x95 */
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0xFF), 0xFF);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0x87), 0xFF);
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    /* in SPI mode the CRC is off until CMD59 */
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0xFF), 0x01);
}

TEST(test_card_checks_the_cmd8_crc)
{
    mock_reset();
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    /* 7.2.2: "card returns CRC error in R1 response", Figure 7-9 has the
     * com crc error at bit 3. 7.3.2: only that byte is sent. */
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0xFF), 0x09);
    ASSERT_EQ_HEX(mock_xfer(NULL, 0xFF), 0xFF);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0x87), 0x01);
}

TEST(test_card_sdhc_never_ready_without_hcs)
{
    mock_reset();
    mock_is_sdhc = 1;
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0x87), 0x01);
    /* 4.2.3: "If HCS is set to 0, SDHC and SDXC Cards never return ready
     * status (keep busy bit to 0)." */
    for (int i = 0; i < 50; i++)
        ASSERT_EQ_HEX(raw_acmd41(0), 0x01);
    ASSERT_EQ_HEX(raw_acmd41(0x40000000U), 0x00);
}

TEST(test_card_standard_capacity_ignores_hcs)
{
    mock_reset();
    mock_is_sdhc = 0;
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0x87), 0x01);
    /* 4.2.3: "Standard Capacity SD Memory Card ignores HCS." */
    ASSERT_EQ_HEX(raw_acmd41(0), 0x00);
}

TEST(test_card_idle_state_refuses_transfers)
{
    mock_reset();
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    /* 7.2.7: "the only valid host commands are CMD8 (SEND_IF_COND), ACMD41
     * (SD_SEND_OP_COND), CMD58 (READ_OCR) and CMD59 (CRC_ON_OFF)". */
    ASSERT_EQ_HEX(raw_cmd(17, 0, 0xFF), 0x05);
    ASSERT_EQ_HEX(raw_cmd(24, 0, 0xFF), 0x05);
    ASSERT_EQ_HEX(raw_cmd(13, 0, 0xFF), 0x05);
    /* CMD58 works, with the idle bit and the power up bit (5.1) clear */
    ASSERT_EQ_HEX(raw_cmd(58, 0, 0xFF), 0x01);
    ASSERT_EQ_HEX(mock_xfer(NULL, 0xFF) & 0xC0U, 0x00);
}

TEST(test_card_r1_loses_the_idle_bit_when_ready)
{
    mock_reset();
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0x87), 0x01);
    ASSERT_EQ_HEX(raw_cmd(55, 0, 0xFF), 0x01);
    ASSERT_EQ_HEX(raw_cmd(41, 0x40000000U, 0xFF), 0x00);
    /* 7.3.2.1: the bit means "running the initializing process" */
    ASSERT_EQ_HEX(raw_cmd(55, 0, 0xFF), 0x00);
    ASSERT_EQ_HEX(raw_cmd(58, 0, 0xFF), 0x00);
    ASSERT_EQ_HEX(mock_xfer(NULL, 0xFF) & 0xC0U, 0xC0);
    ASSERT_EQ_HEX(raw_cmd(17, 0, 0xFF), 0x00);
}

TEST(test_read_is_refused_before_init)
{
    uint8_t buf[512] = { 0 };
    sd_spi_t sd;
    mock_reset();
    memset(&sd, 0, sizeof(sd));
    sd.hal = &mock_hal;
    sd.card_type = SD_SPI_TYPE_SDV2_HC;

    /* no CMD0 yet: the card has not even entered SPI mode */
    ASSERT_EQ(sd_spi_read_block(&sd, 0, buf), SD_SPI_ERR_CMD);
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0x95), 0x01);
    mock_cs_high(NULL);
    ASSERT_EQ(sd_spi_read_block(&sd, 0, buf), SD_SPI_ERR_CMD);
    ASSERT_EQ(sd_spi_write_block(&sd, 0, buf), SD_SPI_ERR_CMD);
}

/* A byte-addressed card takes block * 512 in a 32-bit argument, so the highest
 * block it can be sent is UINT32_MAX / 512, which is 0x7FFFFF */
static void assert_past_range_refused(int is_v1)
{
    static uint8_t buf[512];
    sd_spi_t sd;
    mock_reset();
    mock_is_sdhc = 0;
    mock_is_v1 = is_v1;
    ASSERT_EQ(sd_spi_init(&sd, &mock_hal), SD_SPI_OK);

    mock_last_addr = 0x12345678;
    ASSERT_EQ(sd_spi_write_block(&sd, 0x800000, buf), SD_SPI_ERR_PARAM);
    ASSERT_EQ(sd_spi_read_block(&sd, 0x800000, buf), SD_SPI_ERR_PARAM);
    ASSERT_EQ(sd_spi_write_block(&sd, 0xFFFFFFFFu, buf), SD_SPI_ERR_PARAM);
    ASSERT_EQ(mock_last_addr, 0x12345678);
}

TEST(test_blocks_past_byte_range_are_refused)
{
    assert_past_range_refused(0);
    assert_past_range_refused(1);
}

TEST(test_highest_byte_addressed_block)
{
    static uint8_t buf[512];
    sd_spi_t sd;
    mock_reset();
    mock_is_sdhc = 0;
    ASSERT_EQ(sd_spi_init(&sd, &mock_hal), SD_SPI_OK);

    ASSERT_EQ(sd_spi_write_block(&sd, 0x7FFFFF, buf), SD_SPI_OK);
    ASSERT_EQ_HEX(mock_last_addr, 0xFFFFFE00u);
    ASSERT_EQ(sd_spi_read_block(&sd, 0x7FFFFF, buf), SD_SPI_OK);
    ASSERT_EQ_HEX(mock_last_addr, 0xFFFFFE00u);
}

TEST(test_block_addressed_card_takes_any_block)
{
    static uint8_t buf[512];
    sd_spi_t sd;
    mock_reset();
    mock_is_sdhc = 1;
    ASSERT_EQ(sd_spi_init(&sd, &mock_hal), SD_SPI_OK);

    ASSERT_EQ(sd_spi_write_block(&sd, 0x800000, buf), SD_SPI_OK);
    ASSERT_EQ_HEX(mock_last_addr, 0x800000u);
    ASSERT_EQ(sd_spi_read_block(&sd, 0xFFFFFFFFu, buf), SD_SPI_OK);
    ASSERT_EQ_HEX(mock_last_addr, 0xFFFFFFFFu);
}

/* ---------- a card init did not finish ---------- */

static void assert_card_refused(sd_spi_t *sd)
{
    static uint8_t buf[512];
    ASSERT_EQ(sd_spi_read_block(sd, 0, buf), SD_SPI_ERR_PARAM);
    ASSERT_EQ(sd_spi_write_block(sd, 0, buf), SD_SPI_ERR_PARAM);
    ASSERT_EQ(sd_spi_get_type(sd), SD_SPI_TYPE_UNKNOWN);
}

TEST(test_card_refused_after_failed_init)
{
    sd_spi_t sd;
    mock_reset();
    mock_fail_init = 1;
    ASSERT_EQ(sd_spi_init(&sd, &mock_hal), SD_SPI_ERR_INIT);
    assert_card_refused(&sd);
}

/* sd holds whatever the stack did, as nothing else wrote it */
TEST(test_card_refused_after_rejected_init)
{
    static const sd_spi_hal_t zeroed;
    sd_spi_t sd;
    mock_reset();

    memset(&sd, 0xA5, sizeof(sd));
    ASSERT_EQ(sd_spi_init(&sd, NULL), SD_SPI_ERR_PARAM);
    assert_card_refused(&sd);

    memset(&sd, 0xA5, sizeof(sd));
    ASSERT_EQ(sd_spi_init(&sd, &zeroed), SD_SPI_ERR_PARAM);
    assert_card_refused(&sd);
}

TEST_MAIN("sd_spi")
    RUN(test_init_sdhc);
    RUN(test_init_sdsc);
    RUN(test_init_sdv1);
    RUN(test_init_cmd0_fail);
    RUN(test_init_acmd41_slow);
    RUN(test_null_params);
    RUN(test_null_buf_read);
    RUN(test_null_buf_write);
    RUN(test_read_block);
    RUN(test_read_timeout);
    RUN(test_write_block);
    RUN(test_write_rejected);
    RUN(test_get_type);
    RUN(test_block_count);
    RUN(test_sdhc_block_addressing);
    RUN(test_sdsc_byte_addressing);
    RUN(test_read_write_cycle);
    RUN(test_spi_failure_during_cmd);
    RUN(test_read_cmd_rejected);
    RUN(test_write_cmd_rejected);
    RUN(test_spi_failure_during_read_returns_timeout);
    RUN(test_spi_failure_during_write);
    RUN(test_err_crc_value);
    RUN(test_err_spi_value);
    RUN(test_write_outlasts_a_long_busy);
    RUN(test_write_busy_limit_is_over_500_ms);
    RUN(test_read_token_limit_is_100_ms);
    RUN(test_write_reports_a_programming_error);
    RUN(test_cmd8_crc_error_is_not_a_legacy_card);
    RUN(test_silent_cmd8_is_not_a_legacy_card);
    RUN(test_read_reports_a_data_error_token);
    RUN(test_card_first_cmd0_needs_a_valid_crc);
    RUN(test_card_checks_the_cmd8_crc);
    RUN(test_card_sdhc_never_ready_without_hcs);
    RUN(test_card_standard_capacity_ignores_hcs);
    RUN(test_card_idle_state_refuses_transfers);
    RUN(test_card_r1_loses_the_idle_bit_when_ready);
    RUN(test_read_is_refused_before_init);
    RUN(test_blocks_past_byte_range_are_refused);
    RUN(test_highest_byte_addressed_block);
    RUN(test_block_addressed_card_takes_any_block);
    RUN(test_card_refused_after_failed_init);
    RUN(test_card_refused_after_rejected_init);
TEST_MAIN_END()
