// SPDX-License-Identifier: MIT
/*
 * gps_tracker -- integration recipe: GPS tracking with time sync & logging
 *
 * Demonstrates seven wheels working together:
 *   L0  kalman_1d     -- filter noisy GPS positions
 *   L0  base64        -- encode binary data for transmission
 *   L0  uuid          -- generate unique track session IDs
 *   L1  ds3231        -- RTC for timestamping
 *   L1  sd_spi        -- SD card logging
 *   L2  sntp_client   -- NTP time sync (packet framing)
 *   L2  ubx_parser    -- UBX protocol GPS parsing
 *
 * Runs entirely on host with mock I2C/SPI/UART -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/gps_tracker
 */

#include "test_harness.h"

/* wheels */
#include "kalman_1d.h"
#include "base64.h"
#include "uuid.h"
#include "ds3231.h"
#include "sd_spi.h"
#include "sntp_client.h"
#include "ubx_parser.h"

#include <string.h>
#include <math.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define NUM_NOISY_READINGS  5
#define SD_MOCK_BLOCKS      4     /* mock SD card: 4 blocks of 512 bytes */

/* ======================================================================
 * Mock I2C -- DS3231 (register-based: write_reg / read_regs)
 *
 * Registers 0x00-0x06: time in BCD (sec/min/hr/day/date/mon/yr)
 * Register 0x11-0x12: temperature (MSB + fraction)
 * ====================================================================== */

static uint8_t ds_regs[256];

static uint8_t bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

static void ds_mock_reset(void)
{
    memset(ds_regs, 0, sizeof(ds_regs));

    /* Plant time: 2026-05-12 14:30:45, Monday (weekday=1) */
    ds_regs[0x00] = bin_to_bcd(45);   /* seconds */
    ds_regs[0x01] = bin_to_bcd(30);   /* minutes */
    ds_regs[0x02] = bin_to_bcd(14);   /* hours (24h mode, bit6=0) */
    ds_regs[0x03] = 1;                /* weekday: Monday */
    ds_regs[0x04] = bin_to_bcd(12);   /* date */
    ds_regs[0x05] = bin_to_bcd(5);    /* month */
    ds_regs[0x06] = bin_to_bcd(26);   /* year (2026) */

    /* Temperature: 23.25C (MSB=23, fraction=01 -> 0.25) */
    ds_regs[0x11] = 23;
    ds_regs[0x12] = 0x40;  /* bits 7:6 = 01 -> 0.25C */
}

static bool ds_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    ds_regs[reg] = val;
    return true;
}

static bool ds_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = ds_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const ds3231_i2c_t ds_i2c = {
    .write_reg = ds_write_reg,
    .read_regs = ds_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock SPI -- SD card
 *
 * Simulates the SD card init handshake (CMD0/CMD8/ACMD41/CMD58) and
 * block read/write via a RAM buffer.
 * ====================================================================== */

static uint8_t sd_storage[SD_MOCK_BLOCKS * SD_SPI_BLOCK_SIZE];

/*
 * SD SPI mock state machine.
 *
 * send_cmd() in sd_spi.c does:
 *   cs_low -> xfer(0xFF) [dummy] -> xfer(0x40|cmd) -> 4 arg bytes -> CRC
 *   then polls xfer(0xFF) until R1 response (bit7=0).
 *
 * We detect the command start byte (bit6 set = 0x40) to begin collecting
 * the 6-byte command frame, then queue the response.
 */
static struct {
    bool     cs_asserted;
    uint8_t  cmd_buf[6];        /* 6-byte frame: (0x40|cmd) arg[4] crc */
    uint8_t  cmd_idx;
    bool     collecting_cmd;    /* true once we see 0x40 bit */
    bool     cmd_ready;         /* command fully collected */
    uint8_t  response_buf[8];   /* queued response bytes */
    uint8_t  resp_idx;
    uint8_t  resp_len;
    /* write state */
    bool     awaiting_data_token;
    uint16_t write_idx;
    uint32_t write_addr;
    bool     writing_data;
    uint16_t write_crc_remaining;
    /* read state */
    bool     reading_data;
    uint16_t read_idx;
    uint32_t read_addr;
    uint16_t read_crc_remaining;
    bool     read_token_sent;
    /* init state */
    bool     initialized;       /* after ACMD41 success */
} sd_mock;

static void sd_mock_reset(void)
{
    memset(sd_storage, 0, sizeof(sd_storage));
    memset(&sd_mock, 0, sizeof(sd_mock));
}

static void sd_cs_low(void *ctx)
{
    (void)ctx;
    sd_mock.cs_asserted = true;
    sd_mock.cmd_idx = 0;
    sd_mock.collecting_cmd = false;
    sd_mock.cmd_ready = false;
    sd_mock.resp_idx = 0;
    sd_mock.resp_len = 0;
}

static void sd_cs_high(void *ctx)
{
    (void)ctx;
    sd_mock.cs_asserted = false;
    sd_mock.collecting_cmd = false;
    sd_mock.cmd_ready = false;
    sd_mock.reading_data = false;
    sd_mock.writing_data = false;
    sd_mock.awaiting_data_token = false;
}

static void sd_process_cmd(void)
{
    uint8_t cmd = sd_mock.cmd_buf[0] & 0x3Fu;
    uint32_t arg = ((uint32_t)sd_mock.cmd_buf[1] << 24) |
                   ((uint32_t)sd_mock.cmd_buf[2] << 16) |
                   ((uint32_t)sd_mock.cmd_buf[3] << 8)  |
                   ((uint32_t)sd_mock.cmd_buf[4]);

    switch (cmd) {
    case 0:   /* CMD0: GO_IDLE */
        sd_mock.response_buf[0] = 0x01;
        sd_mock.resp_len = 1;
        break;
    case 8:   /* CMD8: SEND_IF_COND -- R7 response */
        sd_mock.response_buf[0] = 0x01;
        sd_mock.response_buf[1] = 0x00;
        sd_mock.response_buf[2] = 0x00;
        sd_mock.response_buf[3] = 0x01;
        sd_mock.response_buf[4] = 0xAA;
        sd_mock.resp_len = 5;
        break;
    case 55:  /* CMD55: APP_CMD prefix */
        sd_mock.response_buf[0] = sd_mock.initialized ? 0x00 : 0x01;
        sd_mock.resp_len = 1;
        break;
    case 41:  /* ACMD41: SD_SEND_OP_COND */
        sd_mock.initialized = true;
        sd_mock.response_buf[0] = 0x00;
        sd_mock.resp_len = 1;
        break;
    case 58:  /* CMD58: READ_OCR -- SDHC (CCS bit set) */
        sd_mock.response_buf[0] = 0x00;
        sd_mock.response_buf[1] = 0xC0;  /* CCS=1 */
        sd_mock.response_buf[2] = 0xFF;
        sd_mock.response_buf[3] = 0x80;
        sd_mock.response_buf[4] = 0x00;
        sd_mock.resp_len = 5;
        break;
    case 17:  /* CMD17: READ_SINGLE_BLOCK */
        sd_mock.response_buf[0] = 0x00;
        sd_mock.resp_len = 1;
        sd_mock.reading_data = true;
        sd_mock.read_token_sent = false;
        sd_mock.read_idx = 0;
        sd_mock.read_addr = arg;
        sd_mock.read_crc_remaining = 2;
        break;
    case 24:  /* CMD24: WRITE_BLOCK */
        sd_mock.response_buf[0] = 0x00;
        sd_mock.resp_len = 1;
        sd_mock.awaiting_data_token = true;
        sd_mock.write_addr = arg;
        break;
    default:
        sd_mock.response_buf[0] = 0x04;  /* illegal command */
        sd_mock.resp_len = 1;
        break;
    }
    sd_mock.resp_idx = 0;
}

static uint8_t sd_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    if (!sd_mock.cs_asserted) {
        return 0xFF;
    }

    /* Queued response bytes from last command (R1, R7, etc.)
     * Must be returned BEFORE entering data transfer phases. */
    if (sd_mock.resp_idx < sd_mock.resp_len) {
        return sd_mock.response_buf[sd_mock.resp_idx++];
    }

    /* Streaming a read block: data token -> 512 bytes -> 2 CRC */
    if (sd_mock.reading_data) {
        if (!sd_mock.read_token_sent) {
            sd_mock.read_token_sent = true;
            return 0xFE;  /* data token */
        }
        if (sd_mock.read_idx < SD_SPI_BLOCK_SIZE) {
            uint32_t offset = sd_mock.read_addr * SD_SPI_BLOCK_SIZE +
                              sd_mock.read_idx;
            sd_mock.read_idx++;
            if (offset < sizeof(sd_storage)) {
                return sd_storage[offset];
            }
            return 0xFF;
        }
        if (sd_mock.read_crc_remaining > 0) {
            sd_mock.read_crc_remaining--;
            return 0x00;
        }
        return 0xFF;
    }

    /* Waiting for data token (0xFE) before write block */
    if (sd_mock.awaiting_data_token) {
        if (tx == 0xFE) {
            sd_mock.awaiting_data_token = false;
            sd_mock.writing_data = true;
            sd_mock.write_idx = 0;
            sd_mock.write_crc_remaining = 2;
        }
        return 0xFF;
    }

    /* Writing a data block: 512 bytes -> 2 CRC bytes -> data response.
     * sd_spi.c sends: 512 data, 2x xfer(0xFF) for CRC, then reads
     * resp = xfer(0xFF). So CRC bytes return 0xFF, next byte = 0x05. */
    if (sd_mock.writing_data) {
        if (sd_mock.write_idx < SD_SPI_BLOCK_SIZE) {
            uint32_t offset = sd_mock.write_addr * SD_SPI_BLOCK_SIZE +
                              sd_mock.write_idx;
            if (offset < sizeof(sd_storage)) {
                sd_storage[offset] = tx;
            }
            sd_mock.write_idx++;
            return 0xFF;
        }
        /* After 512 data bytes: 2 CRC bytes, then data response byte */
        if (sd_mock.write_crc_remaining > 0) {
            sd_mock.write_crc_remaining--;
            return 0xFF;  /* absorb CRC bytes */
        }
        /* Next xfer after CRC: return data response, then go idle */
        sd_mock.writing_data = false;
        return 0x05;  /* data accepted */
    }

    /* Detect command start: bit 6 set (0x40|cmd) */
    if (!sd_mock.collecting_cmd && (tx & 0xC0u) == 0x40u) {
        sd_mock.collecting_cmd = true;
        sd_mock.cmd_idx = 0;
        sd_mock.cmd_buf[sd_mock.cmd_idx++] = tx;
        return 0xFF;
    }

    /* Collecting remaining 5 bytes of command frame */
    if (sd_mock.collecting_cmd) {
        sd_mock.cmd_buf[sd_mock.cmd_idx++] = tx;
        if (sd_mock.cmd_idx >= 6) {
            sd_mock.collecting_cmd = false;
            sd_process_cmd();
        }
        return 0xFF;
    }

    return 0xFF;  /* idle / dummy bytes */
}

static void sd_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static const sd_spi_hal_t sd_hal = {
    .cs_low   = sd_cs_low,
    .cs_high  = sd_cs_high,
    .xfer     = sd_xfer,
    .delay_ms = sd_delay_ms,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock RNG for UUID generation
 * ====================================================================== */

static uint8_t rng_counter;

static void mock_rng(void *ctx, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = rng_counter++;
    }
}

/* ======================================================================
 * Mock GPS -- build a UBX NAV-PVT message with known position
 *
 * NAV-PVT payload is 92 bytes. Key fields:
 *   offset 24-27: lon (int32_t, 1e-7 degrees)
 *   offset 28-31: lat (int32_t, 1e-7 degrees)
 *   offset 36-39: hMSL (int32_t, mm)
 *   offset 60:    fixType (3 = 3D fix)
 *   offset 21:    valid flags
 * ====================================================================== */

#define NAV_PVT_PAYLOAD_LEN  92

/* Reference position: Shanghai, China (31.2304 N, 121.4737 E, 4m) */
#define REF_LAT_E7   312304000L   /* 31.2304 * 1e7 */
#define REF_LON_E7  1214737000L   /* 121.4737 * 1e7 */
#define REF_ALT_MM        4000L   /* 4.0 m in mm */

static uint8_t mock_ubx_frame[NAV_PVT_PAYLOAD_LEN + UBX_OVERHEAD];
static int     mock_ubx_frame_len;

static void build_mock_nav_pvt(void)
{
    uint8_t payload[NAV_PVT_PAYLOAD_LEN];
    memset(payload, 0, sizeof(payload));

    /* fixType at offset 20: 3 = 3D fix */
    payload[20] = 3;

    /* valid flags at offset 21: date+time+fullyResolved */
    payload[21] = 0x07;

    /* lon at offset 24 (little-endian int32) */
    int32_t lon = (int32_t)REF_LON_E7;
    memcpy(&payload[24], &lon, 4);

    /* lat at offset 28 (little-endian int32) */
    int32_t lat = (int32_t)REF_LAT_E7;
    memcpy(&payload[28], &lat, 4);

    /* hMSL at offset 36 (little-endian int32, mm) */
    int32_t alt = (int32_t)REF_ALT_MM;
    memcpy(&payload[36], &alt, 4);

    /* Build complete UBX frame with correct checksum */
    mock_ubx_frame_len = ubx_parser_build(
        mock_ubx_frame, sizeof(mock_ubx_frame),
        UBX_CLASS_NAV, UBX_NAV_PVT,
        payload, NAV_PVT_PAYLOAD_LEN);
}

/* ======================================================================
 * GPS fix record -- what gets base64-encoded and written to SD
 * ====================================================================== */

typedef struct {
    int32_t  lat_e7;
    int32_t  lon_e7;
    int32_t  alt_mm;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  fix_type;
} gps_fix_record_t;

/* ======================================================================
 * Global instances
 * ====================================================================== */

static kalman_1d_t  kf_lat;
static kalman_1d_t  kf_lon;
static ds3231_t     rtc;
static sd_spi_t     sd;
static uuid_t       session_id;

/* UBX parser context */
static uint8_t          ubx_buf[512];
static ubx_parser_ctx_t ubx_ctx;

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. UUID -- generate session ID */
    rng_counter = 0x42;
    uuid_err_t ue = uuid_v4_generate(&session_id, mock_rng, NULL);
    ASSERT_EQ(ue, UUID_OK);
    ASSERT_FALSE(uuid_is_nil(&session_id));

    /* 2. DS3231 -- RTC init */
    ds_mock_reset();
    ds3231_err_t de = ds3231_init(&rtc, &ds_i2c);
    ASSERT_EQ(de, DS3231_OK);

    /* 3. SD card -- init (exercises full CMD0/CMD8/ACMD41/CMD58 sequence) */
    sd_mock_reset();
    sd_spi_err_t se = sd_spi_init(&sd, &sd_hal);
    ASSERT_EQ(se, SD_SPI_OK);
    ASSERT_EQ(sd_spi_get_type(&sd), SD_SPI_TYPE_SDV2_HC);

    /* 4. UBX parser */
    ubx_parser_err_t pe = ubx_parser_init(&ubx_ctx, ubx_buf, sizeof(ubx_buf));
    ASSERT_EQ(pe, UBX_PARSER_OK);

    /* 5. Kalman filters for lat/lon */
    kalman_1d_cfg_t kcfg = { .q = 0.001f, .r = 0.1f };
    ASSERT_EQ(kalman_1d_init(&kf_lat, &kcfg), KALMAN_1D_OK);
    ASSERT_EQ(kalman_1d_init(&kf_lon, &kcfg), KALMAN_1D_OK);

    /* 6. SNTP -- stateless, no init needed */
    /* 7. Base64 -- stateless, no init needed */

    printf("\n  All 7 wheels initialized successfully.\n");
}

TEST(test_uuid_generate)
{
    /* Verify UUID is valid v4 format */
    char str[37];
    uuid_err_t err = uuid_to_string(&session_id, str, sizeof(str));
    ASSERT_EQ(err, UUID_OK);

    /* UUID string: 8-4-4-4-12 hex = 36 chars */
    ASSERT_EQ(strlen(str), 36);

    /* Verify version nibble (position 14) is '4' */
    ASSERT_EQ(str[14], '4');

    /* Verify dashes at correct positions */
    ASSERT_EQ(str[8], '-');
    ASSERT_EQ(str[13], '-');
    ASSERT_EQ(str[18], '-');
    ASSERT_EQ(str[23], '-');

    printf("  Session UUID: %s\n", str);
}

TEST(test_ds3231_read_time)
{
    ds3231_time_t time;
    ds3231_err_t err = ds3231_get_time(&rtc, &time);
    ASSERT_EQ(err, DS3231_OK);

    /* Verify BCD-decoded time matches planted values */
    ASSERT_EQ(time.year, 26);
    ASSERT_EQ(time.month, 5);
    ASSERT_EQ(time.day, 12);
    ASSERT_EQ(time.hour, 14);
    ASSERT_EQ(time.minute, 30);
    ASSERT_EQ(time.second, 45);
    ASSERT_EQ(time.weekday, 1);

    printf("  RTC time: 20%02u-%02u-%02u %02u:%02u:%02u\n",
           (unsigned)time.year, (unsigned)time.month, (unsigned)time.day,
           (unsigned)time.hour, (unsigned)time.minute, (unsigned)time.second);
}

TEST(test_ubx_parse_nav_pvt)
{
    /* Build mock NAV-PVT frame */
    build_mock_nav_pvt();
    ASSERT_TRUE(mock_ubx_frame_len > 0);

    /* Feed frame byte-by-byte into incremental parser */
    ubx_parser_reset(&ubx_ctx);
    ubx_feed_status_t status = UBX_FEED_IDLE;
    for (int i = 0; i < mock_ubx_frame_len; i++) {
        status = ubx_parser_feed(&ubx_ctx, mock_ubx_frame[i]);
    }
    ASSERT_EQ(status, UBX_FEED_COMPLETE);

    /* Extract parsed message */
    ubx_msg_t msg;
    ubx_parser_err_t err = ubx_parser_get_msg(&ubx_ctx, &msg);
    ASSERT_EQ(err, UBX_PARSER_OK);
    ASSERT_EQ(msg.msg_class, UBX_CLASS_NAV);
    ASSERT_EQ(msg.msg_id, UBX_NAV_PVT);
    ASSERT_EQ(msg.payload_len, NAV_PVT_PAYLOAD_LEN);

    /* Extract lat/lon from payload */
    int32_t parsed_lon, parsed_lat, parsed_alt;
    memcpy(&parsed_lon, &msg.payload[24], 4);
    memcpy(&parsed_lat, &msg.payload[28], 4);
    memcpy(&parsed_alt, &msg.payload[36], 4);

    ASSERT_EQ(parsed_lat, (int32_t)REF_LAT_E7);
    ASSERT_EQ(parsed_lon, (int32_t)REF_LON_E7);
    ASSERT_EQ(parsed_alt, (int32_t)REF_ALT_MM);
    ASSERT_EQ(msg.payload[20], 3);  /* 3D fix */

    printf("  Parsed: lat=%.7f lon=%.7f alt=%.1f m (fix=%u)\n",
           (double)parsed_lat / 1e7, (double)parsed_lon / 1e7,
           (double)parsed_alt / 1e3, (unsigned)msg.payload[20]);
}

TEST(test_kalman_filter_convergence)
{
    /* Simulate 5 noisy lat readings around the reference position.
     * After filtering, the estimate should converge toward the mean. */
    float noisy_lat[] = {
        31.2310f, 31.2298f, 31.2307f, 31.2301f, 31.2304f
    };
    float noisy_lon[] = {
        121.4742f, 121.4731f, 121.4740f, 121.4735f, 121.4737f
    };

    /* Reset filters */
    kalman_1d_reset(&kf_lat);
    kalman_1d_reset(&kf_lon);

    float last_lat = 0.0f, last_lon = 0.0f;
    printf("\n  --- Kalman filter convergence ---\n");
    printf("  step  noisy_lat   filtered    noisy_lon   filtered\n");

    for (int i = 0; i < NUM_NOISY_READINGS; i++) {
        last_lat = kalman_1d_update(&kf_lat, noisy_lat[i]);
        last_lon = kalman_1d_update(&kf_lon, noisy_lon[i]);
        printf("  [%d]   %.4f    %.4f     %.4f    %.4f\n",
               i, (double)noisy_lat[i], (double)last_lat,
               (double)noisy_lon[i], (double)last_lon);
    }

    /* After 5 readings, filtered value should be closer to true position
     * than the first noisy reading was */
    float true_lat = 31.2304f;
    float true_lon = 121.4737f;
    float err_first_lat = fabsf(noisy_lat[0] - true_lat);
    float err_filtered_lat = fabsf(last_lat - true_lat);
    float err_first_lon = fabsf(noisy_lon[0] - true_lon);
    float err_filtered_lon = fabsf(last_lon - true_lon);

    ASSERT_TRUE(err_filtered_lat < err_first_lat);
    ASSERT_TRUE(err_filtered_lon < err_first_lon);

    /* Filtered values should be within 0.001 degrees of truth */
    ASSERT_NEAR(last_lat, true_lat, 0.001f);
    ASSERT_NEAR(last_lon, true_lon, 0.001f);
}

TEST(test_sntp_request_frame)
{
    /* Build an SNTP request packet */
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_timestamp_t tx_time = { .seconds = 0, .fraction = 0 };

    int ret = sntp_client_build_request(pkt, sizeof(pkt), &tx_time);
    ASSERT_EQ(ret, SNTP_PACKET_SIZE);

    /* Verify SNTP v4 client request structure:
     * Byte 0: LI=0 | VN=4 | Mode=3 (client)
     * Expected: (0 << 6) | (4 << 3) | 3 = 0x23 */
    ASSERT_EQ_HEX(pkt[0], 0x23);

    /* Remaining bytes 1-39 should be zero (for a fresh request) */
    for (int i = 1; i < 40; i++) {
        ASSERT_EQ(pkt[i], 0);
    }

    /* Packet should be exactly 48 bytes */
    ASSERT_EQ(ret, 48);

    printf("  SNTP request: LI=%u VN=%u Mode=%u (48 bytes)\n",
           (unsigned)(pkt[0] >> 6), (unsigned)((pkt[0] >> 3) & 0x07),
           (unsigned)(pkt[0] & 0x07));
}

TEST(test_base64_round_trip)
{
    /* Create a GPS fix record */
    gps_fix_record_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.lat_e7   = (int32_t)REF_LAT_E7;
    fix.lon_e7   = (int32_t)REF_LON_E7;
    fix.alt_mm   = (int32_t)REF_ALT_MM;
    fix.hour     = 14;
    fix.minute   = 30;
    fix.second   = 45;
    fix.fix_type = 3;

    /* Encode to base64 */
    uint16_t enc_needed = base64_encoded_size(sizeof(fix));
    char encoded[128];
    uint16_t enc_len = sizeof(encoded);
    base64_err_t err = base64_encode((const uint8_t *)&fix, sizeof(fix),
                                     encoded, &enc_len);
    ASSERT_EQ(err, BASE64_OK);
    ASSERT_TRUE(enc_len > 0);
    ASSERT_EQ(enc_len, enc_needed);

    printf("  Base64 (%u chars): %.60s%s\n", (unsigned)enc_len, encoded,
           enc_len > 60 ? "..." : "");

    /* Decode back */
    gps_fix_record_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    uint16_t dec_len = sizeof(decoded);
    err = base64_decode(encoded, enc_len, (uint8_t *)&decoded, &dec_len);
    ASSERT_EQ(err, BASE64_OK);
    ASSERT_EQ(dec_len, sizeof(fix));

    /* Compare original and decoded */
    ASSERT_EQ(decoded.lat_e7, fix.lat_e7);
    ASSERT_EQ(decoded.lon_e7, fix.lon_e7);
    ASSERT_EQ(decoded.alt_mm, fix.alt_mm);
    ASSERT_EQ(decoded.hour, fix.hour);
    ASSERT_EQ(decoded.minute, fix.minute);
    ASSERT_EQ(decoded.second, fix.second);
    ASSERT_EQ(decoded.fix_type, fix.fix_type);
}

TEST(test_full_track_flow)
{
    /*
     * End-to-end flow:
     *   1. Read RTC time
     *   2. Parse GPS UBX message
     *   3. Filter position through Kalman
     *   4. Build GPS fix record
     *   5. Base64-encode the record
     *   6. Write encoded data to SD block
     *   7. Read back from SD, base64-decode, verify
     */

    printf("\n  --- Full GPS tracking flow ---\n");

    /* 1. Read RTC time */
    ds3231_time_t time;
    ds3231_err_t de = ds3231_get_time(&rtc, &time);
    ASSERT_EQ(de, DS3231_OK);
    printf("  [1] RTC: 20%02u-%02u-%02u %02u:%02u:%02u\n",
           (unsigned)time.year, (unsigned)time.month, (unsigned)time.day,
           (unsigned)time.hour, (unsigned)time.minute, (unsigned)time.second);

    /* 2. Parse GPS UBX message */
    ubx_parser_reset(&ubx_ctx);
    for (int i = 0; i < mock_ubx_frame_len; i++) {
        ubx_parser_feed(&ubx_ctx, mock_ubx_frame[i]);
    }
    ubx_msg_t msg;
    ubx_parser_err_t pe = ubx_parser_get_msg(&ubx_ctx, &msg);
    ASSERT_EQ(pe, UBX_PARSER_OK);

    int32_t raw_lat, raw_lon, raw_alt;
    memcpy(&raw_lat, &msg.payload[28], 4);
    memcpy(&raw_lon, &msg.payload[24], 4);
    memcpy(&raw_alt, &msg.payload[36], 4);
    printf("  [2] GPS raw: lat=%ld lon=%ld alt=%ld mm\n",
           (long)raw_lat, (long)raw_lon, (long)raw_alt);

    /* 3. Filter through Kalman (convert to degrees for filtering) */
    kalman_1d_reset(&kf_lat);
    kalman_1d_reset(&kf_lon);
    float lat_deg = (float)raw_lat / 1e7f;
    float lon_deg = (float)raw_lon / 1e7f;
    float filt_lat = kalman_1d_update(&kf_lat, lat_deg);
    float filt_lon = kalman_1d_update(&kf_lon, lon_deg);
    printf("  [3] Filtered: lat=%.7f lon=%.7f\n",
           (double)filt_lat, (double)filt_lon);

    /* 4. Build fix record */
    gps_fix_record_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.lat_e7   = (int32_t)(filt_lat * 1e7f);
    fix.lon_e7   = (int32_t)(filt_lon * 1e7f);
    fix.alt_mm   = raw_alt;
    fix.hour     = time.hour;
    fix.minute   = time.minute;
    fix.second   = time.second;
    fix.fix_type = msg.payload[20];

    /* 5. Base64-encode */
    char encoded[128];
    uint16_t enc_len = sizeof(encoded);
    base64_err_t be = base64_encode((const uint8_t *)&fix, sizeof(fix),
                                    encoded, &enc_len);
    ASSERT_EQ(be, BASE64_OK);
    printf("  [5] Encoded: %u bytes -> %u base64 chars\n",
           (unsigned)sizeof(fix), (unsigned)enc_len);

    /* 6. Write to SD card block 0 */
    uint8_t sd_block[SD_SPI_BLOCK_SIZE];
    memset(sd_block, 0, sizeof(sd_block));
    memcpy(sd_block, encoded, enc_len);
    sd_block[enc_len] = '\0';  /* null terminate */

    sd_mock_reset();
    sd_spi_err_t se = sd_spi_init(&sd, &sd_hal);
    ASSERT_EQ(se, SD_SPI_OK);

    se = sd_spi_write_block(&sd, 0, sd_block);
    ASSERT_EQ(se, SD_SPI_OK);
    printf("  [6] Written %u chars to SD block 0\n", (unsigned)enc_len);

    /* 7. Read back and verify round-trip */
    uint8_t readback[SD_SPI_BLOCK_SIZE];
    memset(readback, 0, sizeof(readback));
    se = sd_spi_read_block(&sd, 0, readback);
    ASSERT_EQ(se, SD_SPI_OK);

    /* Decode the base64 from SD */
    uint16_t rb_b64_len = (uint16_t)strlen((char *)readback);
    ASSERT_EQ(rb_b64_len, enc_len);

    gps_fix_record_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    uint16_t dec_len = sizeof(decoded);
    be = base64_decode((const char *)readback, rb_b64_len,
                       (uint8_t *)&decoded, &dec_len);
    ASSERT_EQ(be, BASE64_OK);
    ASSERT_EQ(dec_len, sizeof(fix));

    /* Verify all fields match */
    ASSERT_EQ(decoded.lat_e7, fix.lat_e7);
    ASSERT_EQ(decoded.lon_e7, fix.lon_e7);
    ASSERT_EQ(decoded.alt_mm, fix.alt_mm);
    ASSERT_EQ(decoded.hour, fix.hour);
    ASSERT_EQ(decoded.minute, fix.minute);
    ASSERT_EQ(decoded.second, fix.second);
    ASSERT_EQ(decoded.fix_type, fix.fix_type);

    printf("  [7] Round-trip verified: lat=%ld lon=%ld alt=%ld fix=%u\n",
           (long)decoded.lat_e7, (long)decoded.lon_e7,
           (long)decoded.alt_mm, (unsigned)decoded.fix_type);
    printf("  --- Flow complete ---\n");
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("gps_tracker integration")
    RUN(test_init_all);
    RUN(test_uuid_generate);
    RUN(test_ds3231_read_time);
    RUN(test_ubx_parse_nav_pvt);
    RUN(test_kalman_filter_convergence);
    RUN(test_sntp_request_frame);
    RUN(test_base64_round_trip);
    RUN(test_full_track_flow);
TEST_MAIN_END()
