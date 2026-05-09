#include "test_harness.h"
#include <string.h>
#include "rc522.h"

/* ---------- mock SPI with register model ---------- */

#define MOCK_REG_COUNT 64
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_at;
static int     mock_op_count;
static uint8_t mock_cs_state;
static int     mock_byte_idx;
static bool    mock_is_read;
static uint8_t mock_addr_latch;

/* Pending response: data to inject into FIFO when Transceive command starts */
static uint8_t pending_resp[64];
static uint8_t pending_resp_len;
static bool    pending_resp_armed;

/* FIFO simulation for mock */
#define MOCK_FIFO_SIZE 64
static uint8_t mock_fifo[MOCK_FIFO_SIZE];
static uint8_t mock_fifo_head;
static uint8_t mock_fifo_count;

static void mock_fifo_push(uint8_t val)
{
    if (mock_fifo_count < MOCK_FIFO_SIZE) {
        mock_fifo[(mock_fifo_head + mock_fifo_count) % MOCK_FIFO_SIZE] = val;
        mock_fifo_count++;
    }
}

static uint8_t mock_fifo_pop(void)
{
    if (mock_fifo_count == 0) return 0x00;
    uint8_t val = mock_fifo[mock_fifo_head];
    mock_fifo_head = (uint8_t)((mock_fifo_head + 1) % MOCK_FIFO_SIZE);
    mock_fifo_count--;
    return val;
}

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_at   = -1;
    mock_op_count  = 0;
    mock_cs_state  = 1;
    mock_byte_idx  = 0;
    mock_is_read   = false;
    mock_addr_latch = 0;
    memset(mock_fifo, 0, sizeof(mock_fifo));
    mock_fifo_head  = 0;
    mock_fifo_count = 0;
    memset(pending_resp, 0, sizeof(pending_resp));
    pending_resp_len = 0;
    pending_resp_armed = false;

    /* Set version register to v2.0 */
    mock_regs[0x37] = 0x92;
    mock_regs[0x06] = 0x00;  /* no errors */
}

/* Arm a pending response -- injected when Transceive command is written */
static void mock_arm_response(const uint8_t *data, uint8_t len)
{
    memcpy(pending_resp, data, len);
    pending_resp_len = len;
    pending_resp_armed = true;
}

static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; mock_byte_idx = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    mock_op_count++;

    if (mock_byte_idx == 0) {
        mock_is_read = (tx & 0x80u) != 0;
        mock_addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
        mock_byte_idx++;
        return 0x00;
    }

    uint8_t ret = 0x00;

    if (mock_is_read) {
        if (mock_addr_latch == 0x09) {
            ret = mock_fifo_pop();
            mock_regs[0x0A] = mock_fifo_count;
        } else if (mock_addr_latch < MOCK_REG_COUNT) {
            ret = mock_regs[mock_addr_latch];
        }
        if (tx & 0x80u) {
            mock_addr_latch = (uint8_t)((tx >> 1) & 0x3Fu);
        }
    } else {
        if (mock_addr_latch == 0x01 && tx == 0x0C && pending_resp_armed) {
            /* Transceive command written -- inject pending response */
            mock_fifo_head = 0;
            mock_fifo_count = 0;
            for (uint8_t i = 0; i < pending_resp_len; i++) {
                mock_fifo_push(pending_resp[i]);
            }
            mock_regs[0x0A] = mock_fifo_count;
            mock_regs[0x04] = 0x20;  /* RxIRq set */
            pending_resp_armed = false;
            mock_regs[mock_addr_latch] = tx;
        } else if (mock_addr_latch == 0x09) {
            mock_fifo_push(tx);
            mock_regs[0x0A] = mock_fifo_count;
        } else if (mock_addr_latch == 0x0A && (tx & 0x80u)) {
            mock_fifo_head = 0;
            mock_fifo_count = 0;
            mock_regs[0x0A] = 0;
        } else if (mock_addr_latch < MOCK_REG_COUNT) {
            mock_regs[mock_addr_latch] = tx;
        }
    }

    mock_byte_idx++;
    return ret;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static const rc522_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x92);
}

TEST(test_init_version_v1)
{
    mock_reset();
    mock_regs[0x37] = 0x91;  /* v1.0 */
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x91);
}

TEST(test_init_bad_version)
{
    mock_reset();
    mock_regs[0x37] = 0x00;  /* invalid */
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_ERR_ID);
}

TEST(test_null_safety)
{
    rc522_t dev;
    ASSERT_EQ(rc522_init(NULL, &mock_spi), RC522_ERR_PARAM);
    ASSERT_EQ(rc522_init(&dev, NULL), RC522_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    rc522_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    rc522_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(rc522_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)RC522_ERR_PARAM);
    ASSERT_EQ(-2, (int)RC522_ERR_SPI);
    ASSERT_EQ(-3, (int)RC522_ERR_ID);
    ASSERT_EQ(-4, (int)RC522_ERR_TIMEOUT);
    ASSERT_EQ(-5, (int)RC522_ERR_COLL);
}

TEST(test_get_version)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);
    ASSERT_EQ(rc522_get_version(&dev), 0x92);
    ASSERT_EQ(rc522_get_version(NULL), 0);
}

TEST(test_antenna_on)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);
    ASSERT_EQ(rc522_antenna_on(&dev), RC522_OK);
    /* TxControlReg should have bits 0,1 set */
    ASSERT_TRUE(mock_regs[0x14] & 0x03);
}

TEST(test_antenna_on_null)
{
    ASSERT_EQ(rc522_antenna_on(NULL), RC522_ERR_PARAM);
}

TEST(test_detect_card)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm ATQA response (Mifare 1K: 0x04, 0x00) */
    uint8_t atqa[2] = {0x04, 0x00};
    mock_arm_response(atqa, 2);

    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
}

TEST(test_detect_card_null)
{
    ASSERT_EQ(rc522_detect_card(NULL), RC522_ERR_PARAM);
}

TEST(test_detect_card_timeout)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* No response armed -- should timeout */
    mock_regs[0x04] = 0x01;  /* TimerIRq only */

    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_TIMEOUT);
}

TEST(test_read_uid)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm UID response: 0xDE 0xAD 0xBE 0xEF + BCC */
    uint8_t uid_resp[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0};
    uid_resp[4] = (uint8_t)(0xDE ^ 0xAD ^ 0xBE ^ 0xEF);
    mock_arm_response(uid_resp, 5);

    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_OK);
    ASSERT_EQ(uid.len, 4);
    ASSERT_EQ(uid.bytes[0], 0xDE);
    ASSERT_EQ(uid.bytes[1], 0xAD);
    ASSERT_EQ(uid.bytes[2], 0xBE);
    ASSERT_EQ(uid.bytes[3], 0xEF);
}

TEST(test_read_uid_bad_bcc)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm UID response with wrong BCC */
    uint8_t uid_resp[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF};
    mock_arm_response(uid_resp, 5);

    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_ERR_COLL);
}

TEST(test_read_uid_null)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);
    ASSERT_EQ(rc522_read_card_uid(NULL, &(rc522_uid_t){0}), RC522_ERR_PARAM);
    ASSERT_EQ(rc522_read_card_uid(&dev, NULL), RC522_ERR_PARAM);
}

TEST(test_halt)
{
    mock_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* HALT expects timeout (card stops responding) -- set TimerIRq */
    mock_regs[0x04] = 0x01;  /* TimerIRq set, not RxIRq */

    ASSERT_EQ(rc522_halt(&dev), RC522_OK);
}

TEST(test_halt_null)
{
    ASSERT_EQ(rc522_halt(NULL), RC522_ERR_PARAM);
}

TEST(test_self_test_null)
{
    ASSERT_EQ(rc522_self_test(NULL), RC522_ERR_PARAM);
}

TEST_MAIN("rc522")
    RUN(test_init_ok);
    RUN(test_init_version_v1);
    RUN(test_init_bad_version);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_get_version);
    RUN(test_antenna_on);
    RUN(test_antenna_on_null);
    RUN(test_detect_card);
    RUN(test_detect_card_null);
    RUN(test_detect_card_timeout);
    RUN(test_read_uid);
    RUN(test_read_uid_bad_bcc);
    RUN(test_read_uid_null);
    RUN(test_halt);
    RUN(test_halt_null);
    RUN(test_self_test_null);
TEST_MAIN_END()
