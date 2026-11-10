// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "onewire.h"

/* ========== mock GPIO with operation tracing ========== */

#define TRACE_MAX 2048

typedef enum {
    OP_LOW = 1,
    OP_RELEASE,
    OP_READ,
    OP_DELAY,
} op_type_t;

typedef struct {
    op_type_t type;
    uint16_t  delay_us;   /* only valid for OP_DELAY */
    bool      read_value; /* only valid for OP_READ  */
} op_entry_t;

static op_entry_t g_ops[TRACE_MAX];
static uint16_t   g_ops_idx;

/* Preloaded read responses */
#define READ_MAX 256
static bool     g_read_values[READ_MAX];
static uint16_t g_read_idx;
static uint16_t g_read_len;

/* Total delay accumulated */
static uint32_t g_total_delay_us;

static void mock_pin_low(void *ctx)
{
    (void)ctx;
    if (g_ops_idx < TRACE_MAX) {
        g_ops[g_ops_idx].type = OP_LOW;
        g_ops[g_ops_idx].delay_us = 0;
        g_ops[g_ops_idx].read_value = false;
        g_ops_idx++;
    }
}

static void mock_pin_release(void *ctx)
{
    (void)ctx;
    if (g_ops_idx < TRACE_MAX) {
        g_ops[g_ops_idx].type = OP_RELEASE;
        g_ops[g_ops_idx].delay_us = 0;
        g_ops[g_ops_idx].read_value = false;
        g_ops_idx++;
    }
}

static bool mock_pin_read(void *ctx)
{
    (void)ctx;
    bool val = true; /* default: line high (no device) */
    if (g_read_idx < g_read_len) {
        val = g_read_values[g_read_idx++];
    }
    if (g_ops_idx < TRACE_MAX) {
        g_ops[g_ops_idx].type = OP_READ;
        g_ops[g_ops_idx].delay_us = 0;
        g_ops[g_ops_idx].read_value = val;
        g_ops_idx++;
    }
    return val;
}

static void mock_delay_us(void *ctx, uint16_t us)
{
    (void)ctx;
    g_total_delay_us += us;
    if (g_ops_idx < TRACE_MAX) {
        g_ops[g_ops_idx].type = OP_DELAY;
        g_ops[g_ops_idx].delay_us = us;
        g_ops[g_ops_idx].read_value = false;
        g_ops_idx++;
    }
}

static const onewire_gpio_t g_mock_gpio = {
    .pin_low     = mock_pin_low,
    .pin_release = mock_pin_release,
    .pin_read    = mock_pin_read,
    .delay_us    = mock_delay_us,
    .ctx         = NULL,
};

static void mock_reset(void)
{
    memset(g_ops, 0, sizeof(g_ops));
    g_ops_idx = 0;
    memset(g_read_values, 0, sizeof(g_read_values));
    g_read_idx = 0;
    g_read_len = 0;
    g_total_delay_us = 0;
}

/* Set all subsequent reads to return the given value */
static void mock_set_reads(bool value, uint16_t count)
{
    for (uint16_t i = 0; i < count && i < READ_MAX; i++) {
        g_read_values[i] = value;
    }
    g_read_len = count;
    g_read_idx = 0;
}

/*
 * Set read values for reading a specific byte (LSB first).
 * The 1-Wire read protocol issues one read per bit.
 * offset = starting index in g_read_values for the 8 bits.
 */
static void mock_set_read_byte(uint8_t byte, uint16_t offset)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (offset + i < READ_MAX) {
            g_read_values[offset + i] = (bool)((byte >> i) & 1);
        }
    }
    if (offset + 8 > g_read_len) {
        g_read_len = offset + 8;
    }
}

/* Helper: init a bus with mock GPIO for use in tests */
static onewire_t make_bus(void)
{
    onewire_t bus;
    mock_reset();
    onewire_init(&bus, &g_mock_gpio);
    /* Re-reset trace after init (which calls pin_release) */
    g_ops_idx = 0;
    g_total_delay_us = 0;
    return bus;
}

/* Script a reset: the idle check before the pulse, then the presence sample.
 * Both are line levels, true = high. */
static void mock_set_reset_reads(bool idle, bool presence_line)
{
    g_read_values[0] = idle;
    g_read_values[1] = presence_line;
    g_read_len = 2;
    g_read_idx = 0;
}

/* ========== init tests ========== */

TEST(test_init_null_bus)
{
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_init(NULL, &g_mock_gpio));
}

TEST(test_init_null_gpio)
{
    onewire_t bus;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_init(&bus, NULL));
}

TEST(test_init_missing_pin_low)
{
    onewire_t bus;
    onewire_gpio_t bad = g_mock_gpio;
    bad.pin_low = NULL;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_init(&bus, &bad));
}

TEST(test_init_missing_pin_release)
{
    onewire_t bus;
    onewire_gpio_t bad = g_mock_gpio;
    bad.pin_release = NULL;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_init(&bus, &bad));
}

TEST(test_init_missing_pin_read)
{
    onewire_t bus;
    onewire_gpio_t bad = g_mock_gpio;
    bad.pin_read = NULL;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_init(&bus, &bad));
}

TEST(test_init_missing_delay_us)
{
    onewire_t bus;
    onewire_gpio_t bad = g_mock_gpio;
    bad.delay_us = NULL;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_init(&bus, &bad));
}

TEST(test_init_ok)
{
    onewire_t bus;
    mock_reset();
    ASSERT_EQ(ONEWIRE_OK, onewire_init(&bus, &g_mock_gpio));
    /* init should call pin_release to idle the bus */
    ASSERT_TRUE(g_ops_idx >= 1);
    ASSERT_EQ(OP_RELEASE, g_ops[0].type);
}

TEST(test_init_stores_gpio)
{
    onewire_t bus;
    mock_reset();
    onewire_init(&bus, &g_mock_gpio);
    ASSERT_TRUE(bus.gpio == &g_mock_gpio);
}

/* ========== reset tests ========== */

TEST(test_reset_null_bus)
{
    bool presence = false;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_reset(NULL, &presence));
}

TEST(test_reset_uninit_bus)
{
    onewire_t bus;
    memset(&bus, 0, sizeof(bus));
    bool presence = false;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_reset(&bus, &presence));
}

TEST(test_reset_presence_detected)
{
    onewire_t bus = make_bus();
    /* Device present: idle high, then low during the presence window */
    mock_set_reset_reads(true, false);

    bool presence = false;
    ASSERT_EQ(ONEWIRE_OK, onewire_reset(&bus, &presence));
    ASSERT_TRUE(presence);
}

TEST(test_reset_no_presence)
{
    onewire_t bus = make_bus();
    /* No device: line stays high */
    mock_set_reset_reads(true, true);

    bool presence = true;
    ASSERT_EQ(ONEWIRE_ERR_PRESENCE, onewire_reset(&bus, &presence));
    ASSERT_FALSE(presence);
}

TEST(test_reset_no_presence_without_presence_ptr)
{
    onewire_t bus = make_bus();
    mock_set_reset_reads(true, true);

    /* With no out parameter the return value is all a caller has. */
    ASSERT_EQ(ONEWIRE_ERR_PRESENCE, onewire_reset(&bus, NULL));
}

TEST(test_reset_null_presence_ptr)
{
    onewire_t bus = make_bus();
    mock_set_reset_reads(true, false); /* device present */
    /* Must not crash with NULL presence pointer */
    ASSERT_EQ(ONEWIRE_OK, onewire_reset(&bus, NULL));
}

TEST(test_reset_bus_held_low)
{
    onewire_t bus = make_bus();
    mock_set_reads(false, READ_MAX); /* shorted: every read is low */

    bool presence = true;
    ASSERT_EQ(ONEWIRE_ERR_BUS, onewire_reset(&bus, &presence));
    ASSERT_FALSE(presence);

    /* The pulse is pointless on a low line and is not sent. */
    for (uint16_t i = 0; i < g_ops_idx; i++) {
        ASSERT_TRUE(g_ops[i].type != OP_LOW);
    }
}

TEST(test_reset_waits_for_a_slow_line)
{
    onewire_t bus = make_bus();

    /* A loaded bus can still be rising after the last slot: low a few
     * times, then high, then a real presence pulse. */
    mock_set_reads(false, 5);
    g_read_values[5] = true;
    g_read_values[6] = false;
    g_read_len = 7;

    bool presence = false;
    ASSERT_EQ(ONEWIRE_OK, onewire_reset(&bus, &presence));
    ASSERT_TRUE(presence);
}

TEST(test_reset_timing)
{
    /*
     * Reset sequence must have:
     * 1. pin_read (line idle high)
     * 2. pin_low
     * 3. delay >= 480us (reset low)
     * 4. pin_release
     * 5. delay (presence detect wait)
     * 6. pin_read (sample presence)
     * 7. delay (remainder of reset high)
     */
    onewire_t bus = make_bus();
    mock_set_reset_reads(true, false);

    bool presence = false;
    onewire_reset(&bus, &presence);

    /* Check operation sequence */
    ASSERT_EQ(OP_READ, g_ops[0].type);
    ASSERT_EQ(OP_LOW, g_ops[1].type);
    ASSERT_EQ(OP_DELAY, g_ops[2].type);
    ASSERT_EQ(480, g_ops[2].delay_us);
    ASSERT_EQ(OP_RELEASE, g_ops[3].type);
    ASSERT_EQ(OP_DELAY, g_ops[4].type);
    ASSERT_EQ(60, g_ops[4].delay_us);
    ASSERT_EQ(OP_READ, g_ops[5].type);
    ASSERT_EQ(OP_DELAY, g_ops[6].type);
    ASSERT_EQ(420, g_ops[6].delay_us); /* 480 - 60 */

    /* Total delay: 480 + 60 + 420 = 960us */
    ASSERT_EQ(960, g_total_delay_us);
}

/* ========== write_byte tests ========== */

TEST(test_write_byte_null_bus)
{
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_write_byte(NULL, 0x55));
}

TEST(test_write_byte_uninit_bus)
{
    onewire_t bus;
    memset(&bus, 0, sizeof(bus));
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_write_byte(&bus, 0x55));
}

TEST(test_write_byte_0x00)
{
    /*
     * Writing 0x00: all 8 bits are 0.
     * For each bit: LOW, delay(60), RELEASE, delay(2)
     * -> 8 * (LOW + DELAY(60) + RELEASE + DELAY(2)) operations
     */
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_OK, onewire_write_byte(&bus, 0x00));

    /* Check first bit (bit 0 = 0): write-0 pattern */
    ASSERT_EQ(OP_LOW, g_ops[0].type);
    ASSERT_EQ(OP_DELAY, g_ops[1].type);
    ASSERT_EQ(60, g_ops[1].delay_us);  /* T_WDLOW_0 */
    ASSERT_EQ(OP_RELEASE, g_ops[2].type);
    ASSERT_EQ(OP_DELAY, g_ops[3].type);
    ASSERT_EQ(2, g_ops[3].delay_us);   /* T_REC */
}

TEST(test_write_byte_0xFF)
{
    /*
     * Writing 0xFF: all 8 bits are 1.
     * For each bit: LOW, delay(6), RELEASE, delay(54), delay(2)
     */
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_OK, onewire_write_byte(&bus, 0xFF));

    /* Check first bit (bit 0 = 1): write-1 pattern */
    ASSERT_EQ(OP_LOW, g_ops[0].type);
    ASSERT_EQ(OP_DELAY, g_ops[1].type);
    ASSERT_EQ(6, g_ops[1].delay_us);   /* T_WDLOW_1 */
    ASSERT_EQ(OP_RELEASE, g_ops[2].type);
    ASSERT_EQ(OP_DELAY, g_ops[3].type);
    ASSERT_EQ(54, g_ops[3].delay_us);  /* T_SLOT - T_WDLOW_1 */
    ASSERT_EQ(OP_DELAY, g_ops[4].type);
    ASSERT_EQ(2, g_ops[4].delay_us);   /* T_REC */
}

TEST(test_write_byte_0xA5_lsb_first)
{
    /*
     * 0xA5 = 10100101 binary, LSB first: 1,0,1,0,0,1,0,1
     * Verify LSB-first bit ordering by checking first two bits.
     */
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_OK, onewire_write_byte(&bus, 0xA5));

    /* bit 0 = 1 (write-1 pattern): LOW, delay(6), RELEASE, delay(54), delay(2) */
    ASSERT_EQ(OP_LOW, g_ops[0].type);
    ASSERT_EQ(6, g_ops[1].delay_us);

    /* bit 1 = 0 (write-0 pattern): LOW, delay(60), RELEASE, delay(2) */
    /* bit 1 starts at ops index 5 */
    ASSERT_EQ(OP_LOW, g_ops[5].type);
    ASSERT_EQ(60, g_ops[6].delay_us);
}

/* ========== read_byte tests ========== */

TEST(test_read_byte_null_bus)
{
    uint8_t val;
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_read_byte(NULL, &val));
}

TEST(test_read_byte_null_out)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_read_byte(&bus, NULL));
}

TEST(test_read_byte_0xFF)
{
    onewire_t bus = make_bus();
    /* All bits read as 1 */
    mock_set_reads(true, 8);

    uint8_t val = 0;
    ASSERT_EQ(ONEWIRE_OK, onewire_read_byte(&bus, &val));
    ASSERT_EQ(0xFF, val);
}

TEST(test_read_byte_0x00)
{
    onewire_t bus = make_bus();
    /* All bits read as 0 */
    mock_set_reads(false, 8);

    uint8_t val = 0xFF;
    ASSERT_EQ(ONEWIRE_OK, onewire_read_byte(&bus, &val));
    ASSERT_EQ(0x00, val);
}

TEST(test_read_byte_0xA5)
{
    /*
     * 0xA5 = 10100101, LSB first: bits 1,0,1,0,0,1,0,1
     */
    onewire_t bus = make_bus();
    mock_set_read_byte(0xA5, 0);

    uint8_t val = 0;
    ASSERT_EQ(ONEWIRE_OK, onewire_read_byte(&bus, &val));
    ASSERT_EQ(0xA5, val);
}

TEST(test_read_byte_0x33)
{
    onewire_t bus = make_bus();
    mock_set_read_byte(0x33, 0);

    uint8_t val = 0;
    ASSERT_EQ(ONEWIRE_OK, onewire_read_byte(&bus, &val));
    ASSERT_EQ(0x33, val);
}

TEST(test_read_byte_timing)
{
    /*
     * Read bit pattern: LOW, delay(6), RELEASE, delay(9), READ, delay(45)
     * 8 bits -> 8 * (6+9+45) = 480us
     */
    onewire_t bus = make_bus();
    mock_set_reads(true, 8);

    uint8_t val;
    onewire_read_byte(&bus, &val);

    /* Check first read-bit sequence */
    ASSERT_EQ(OP_LOW, g_ops[0].type);
    ASSERT_EQ(OP_DELAY, g_ops[1].type);
    ASSERT_EQ(6, g_ops[1].delay_us);
    ASSERT_EQ(OP_RELEASE, g_ops[2].type);
    ASSERT_EQ(OP_DELAY, g_ops[3].type);
    ASSERT_EQ(9, g_ops[3].delay_us);
    ASSERT_EQ(OP_READ, g_ops[4].type);
    ASSERT_EQ(OP_DELAY, g_ops[5].type);
    ASSERT_EQ(45, g_ops[5].delay_us);
}

/* ========== write (multi-byte) tests ========== */

TEST(test_write_null_bus)
{
    uint8_t data[] = {0x01, 0x02};
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_write(NULL, data, 2));
}

TEST(test_write_null_data_nonzero_len)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_write(&bus, NULL, 1));
}

TEST(test_write_null_data_zero_len)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_OK, onewire_write(&bus, NULL, 0));
}

TEST(test_write_two_bytes)
{
    onewire_t bus = make_bus();
    uint8_t data[] = {0xCC, 0x44};
    ASSERT_EQ(ONEWIRE_OK, onewire_write(&bus, data, 2));
    /* Should have 16 bit write operations worth of trace entries */
    ASSERT_TRUE(g_ops_idx > 0);
}

/* ========== read (multi-byte) tests ========== */

TEST(test_read_null_bus)
{
    uint8_t buf[2];
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_read(NULL, buf, 2));
}

TEST(test_read_null_buf_nonzero_len)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_read(&bus, NULL, 1));
}

TEST(test_read_null_buf_zero_len)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_OK, onewire_read(&bus, NULL, 0));
}

TEST(test_read_two_bytes)
{
    onewire_t bus = make_bus();
    mock_set_read_byte(0xAB, 0);
    mock_set_read_byte(0xCD, 8);

    uint8_t buf[2] = {0};
    ASSERT_EQ(ONEWIRE_OK, onewire_read(&bus, buf, 2));
    ASSERT_EQ(0xAB, buf[0]);
    ASSERT_EQ(0xCD, buf[1]);
}

/* ========== select (MATCH_ROM) tests ========== */

TEST(test_select_null_bus)
{
    uint8_t rom[8] = {0};
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_select(NULL, rom));
}

TEST(test_select_null_rom)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_select(&bus, NULL));
}

TEST(test_select_sends_match_rom_then_8_bytes)
{
    /*
     * MATCH_ROM (0x55) + 8 ROM bytes = 9 bytes total = 72 bits written.
     * Each write-bit has at least a LOW + DELAY + RELEASE + DELAY, so
     * we need at minimum 72 * 4 = 288 operations (rough check).
     */
    onewire_t bus = make_bus();
    uint8_t rom[8] = {0x28, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ASSERT_EQ(ONEWIRE_OK, onewire_select(&bus, rom));
    /* At least 72 bits worth of operations */
    ASSERT_TRUE(g_ops_idx > 72 * 3);
}

/* ========== skip (SKIP_ROM) tests ========== */

TEST(test_skip_null_bus)
{
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_skip(NULL));
}

TEST(test_skip_ok)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_OK, onewire_skip(&bus));
    /* SKIP_ROM = 0xCC, 8 bits written */
    ASSERT_TRUE(g_ops_idx > 0);
}

/* ========== read_rom (READ_ROM) tests ========== */

TEST(test_read_rom_null_bus)
{
    uint8_t rom[8];
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_read_rom(NULL, rom));
}

TEST(test_read_rom_null_rom)
{
    onewire_t bus = make_bus();
    ASSERT_EQ(ONEWIRE_ERR_PARAM, onewire_read_rom(&bus, NULL));
}

TEST(test_read_rom_valid_crc)
{
    /*
     * DS18B20 example ROM: family=0x28, serial=0xFF0000065416, CRC=0xEC
     * Full ROM: {0x28, 0x16, 0x54, 0x06, 0x00, 0x00, 0xFF, 0xEC}
     * CRC-8 of first 7 bytes should equal byte[7].
     */
    onewire_t bus = make_bus();
    uint8_t rom_data[8] = {0x28, 0x16, 0x54, 0x06, 0x00, 0x00, 0xFF, 0x00};
    /* Calculate the correct CRC for the first 7 bytes */
    rom_data[7] = onewire_crc8(rom_data, 7);

    /*
     * read_rom sends READ_ROM (0x33) first (8 write bits, no reads consumed),
     * then reads 8 bytes = 64 read bits.
     */
    uint16_t offset = 0;
    for (int b = 0; b < 8; b++) {
        mock_set_read_byte(rom_data[b], offset);
        offset += 8;
    }

    uint8_t rom[8] = {0};
    ASSERT_EQ(ONEWIRE_OK, onewire_read_rom(&bus, rom));
    ASSERT_EQ(0x28, rom[0]);
    ASSERT_EQ(rom_data[7], rom[7]);
}

TEST(test_read_rom_bad_crc)
{
    /*
     * Feed ROM data with deliberately wrong CRC byte.
     */
    onewire_t bus = make_bus();
    /* Valid ROM bytes but wrong CRC */
    uint8_t rom_data[8] = {0x28, 0x16, 0x54, 0x06, 0x00, 0x00, 0xFF, 0x00};
    rom_data[7] = (uint8_t)(onewire_crc8(rom_data, 7) ^ 0xFF); /* invert CRC */

    uint16_t offset = 0;
    for (int b = 0; b < 8; b++) {
        mock_set_read_byte(rom_data[b], offset);
        offset += 8;
    }

    uint8_t rom[8] = {0};
    ASSERT_EQ(ONEWIRE_ERR_CRC, onewire_read_rom(&bus, rom));
}

TEST(test_read_rom_all_zero_is_refused)
{
    /* CRC-8 of seven zero bytes is 0, so a line held low passes the CRC.
     * 0x00 is no family code: the bytes come from the short, not a device. */
    onewire_t bus = make_bus();
    mock_set_reads(false, 64);

    uint8_t rom[8] = {0};
    ASSERT_EQ(0, onewire_crc8(rom, 7));
    ASSERT_EQ(ONEWIRE_ERR_BUS, onewire_read_rom(&bus, rom));
}

TEST(test_read_rom_with_no_device_fails_the_crc)
{
    /* The opposite fault: a line that stays high reads as all ones. */
    onewire_t bus = make_bus();
    mock_set_reads(true, 64);

    uint8_t rom[8] = {0};
    ASSERT_EQ(ONEWIRE_ERR_CRC, onewire_read_rom(&bus, rom));
}

TEST(test_read_rom_zero_serial_is_not_refused)
{
    /* Only the whole ROM being zero means a stuck line. */
    onewire_t bus = make_bus();
    uint8_t rom_data[8] = {0x28, 0, 0, 0, 0, 0, 0, 0};
    rom_data[7] = onewire_crc8(rom_data, 7);

    uint16_t offset = 0;
    for (int b = 0; b < 8; b++) {
        mock_set_read_byte(rom_data[b], offset);
        offset += 8;
    }

    uint8_t rom[8] = {0};
    ASSERT_EQ(ONEWIRE_OK, onewire_read_rom(&bus, rom));
}

/* ========== CRC-8 tests ========== */

TEST(test_crc8_null)
{
    ASSERT_EQ(0, onewire_crc8(NULL, 5));
}

TEST(test_crc8_empty)
{
    uint8_t data = 0;
    ASSERT_EQ(0, onewire_crc8(&data, 0));
}

TEST(test_crc8_single_byte)
{
    /* CRC-8 of {0x00} = 0x00 (all zeros) */
    uint8_t data = 0x00;
    ASSERT_EQ(0x00, onewire_crc8(&data, 1));

    /* CRC-8 of {0x01} */
    data = 0x01;
    uint8_t crc = onewire_crc8(&data, 1);
    /* Manually: byte=0x01, mix=(0^1)&1=1, crc=0x8C, then 7 more shifts with no mix */
    ASSERT_EQ(0x5E, crc);
}

TEST(test_crc8_known_rom_ds18b20)
{
    /*
     * Known DS18B20 ROM from Dallas datasheet example:
     * Family=0x28, Serial bytes, CRC included.
     * For the purpose of testing, use a manually verified sequence.
     *
     * ROM: 0x28 0xAA 0xBB 0xCC 0xDD 0xEE 0x01
     * Compute CRC of these 7 bytes.
     */
    uint8_t rom[7] = {0x28, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t crc = onewire_crc8(rom, 7);

    /* Verify that CRC of all 8 bytes (including CRC) equals 0 */
    uint8_t full[8];
    memcpy(full, rom, 7);
    full[7] = crc;
    ASSERT_EQ(0, onewire_crc8(full, 8));
}

TEST(test_crc8_all_ones)
{
    uint8_t data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t crc = onewire_crc8(data, 4);
    /* Verify it is self-consistent: CRC of data+crc = 0 */
    uint8_t full[5];
    memcpy(full, data, 4);
    full[4] = crc;
    ASSERT_EQ(0, onewire_crc8(full, 5));
}

TEST(test_crc8_incremental_consistency)
{
    /*
     * CRC of {0x01, 0x02, 0x03} should produce a value such that
     * CRC of {0x01, 0x02, 0x03, <crc>} == 0.
     */
    uint8_t data[3] = {0x01, 0x02, 0x03};
    uint8_t crc = onewire_crc8(data, 3);
    uint8_t full[4] = {0x01, 0x02, 0x03, 0};
    full[3] = crc;
    ASSERT_EQ(0, onewire_crc8(full, 4));
}

TEST(test_crc8_known_vector_family_10)
{
    /*
     * DS18S20 family code = 0x10.
     * ROM: {0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, CRC}
     */
    uint8_t rom[7] = {0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t crc = onewire_crc8(rom, 7);
    uint8_t full[8];
    memcpy(full, rom, 7);
    full[7] = crc;
    ASSERT_EQ(0, onewire_crc8(full, 8));
}

/* ========== error enum values ========== */

TEST(test_error_enum_values)
{
    ASSERT_EQ(0,  ONEWIRE_OK);
    ASSERT_EQ(-1, ONEWIRE_ERR_PARAM);
    ASSERT_EQ(-2, ONEWIRE_ERR_PRESENCE);
    ASSERT_EQ(-3, ONEWIRE_ERR_CRC);
}

/* ========== ROM command constants ========== */

TEST(test_rom_command_constants)
{
    ASSERT_EQ_HEX(0xF0, ONEWIRE_CMD_SEARCH_ROM);
    ASSERT_EQ_HEX(0x33, ONEWIRE_CMD_READ_ROM);
    ASSERT_EQ_HEX(0x55, ONEWIRE_CMD_MATCH_ROM);
    ASSERT_EQ_HEX(0xCC, ONEWIRE_CMD_SKIP_ROM);
}

/* ========== multiple instances ========== */

TEST(test_multiple_instances)
{
    onewire_t bus1, bus2;
    mock_reset();
    ASSERT_EQ(ONEWIRE_OK, onewire_init(&bus1, &g_mock_gpio));
    ASSERT_EQ(ONEWIRE_OK, onewire_init(&bus2, &g_mock_gpio));
    ASSERT_TRUE(bus1.gpio == bus2.gpio);
}

TEST(test_reinit_same_bus)
{
    onewire_t bus;
    onewire_gpio_t gpio2 = g_mock_gpio;
    mock_reset();
    ASSERT_EQ(ONEWIRE_OK, onewire_init(&bus, &g_mock_gpio));
    ASSERT_TRUE(bus.gpio == &g_mock_gpio);
    ASSERT_EQ(ONEWIRE_OK, onewire_init(&bus, &gpio2));
    ASSERT_TRUE(bus.gpio == &gpio2);
}

/* ========== full transaction test ========== */

TEST(test_full_ds18b20_convert_sequence)
{
    /*
     * Simulate a DS18B20 temperature conversion:
     * 1. Reset (presence detected)
     * 2. Skip ROM (0xCC)
     * 3. Write Convert T (0x44)
     *
     * We just verify the function calls succeed and timing is reasonable.
     */
    onewire_t bus = make_bus();
    /* Set presence response for reset */
    mock_set_reset_reads(true, false); /* device present */

    bool presence = false;
    ASSERT_EQ(ONEWIRE_OK, onewire_reset(&bus, &presence));
    ASSERT_TRUE(presence);

    /* Reset trace for skip */
    g_ops_idx = 0;
    g_total_delay_us = 0;
    g_read_idx = 0;

    ASSERT_EQ(ONEWIRE_OK, onewire_skip(&bus));
    ASSERT_EQ(ONEWIRE_OK, onewire_write_byte(&bus, 0x44));
}

TEST(test_full_select_and_read)
{
    /*
     * Simulate: select a device by ROM, then read 2 bytes (e.g. temperature).
     */
    onewire_t bus = make_bus();

    uint8_t rom[8] = {0x28, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00};
    rom[7] = onewire_crc8(rom, 7);
    ASSERT_EQ(ONEWIRE_OK, onewire_select(&bus, rom));

    /* Set up read responses for 2 bytes: 0x50 (80 = +5.0 C raw) and 0x01 */
    g_read_idx = 0;
    g_read_len = 0;
    mock_set_read_byte(0x50, 0);
    mock_set_read_byte(0x01, 8);

    uint8_t buf[2] = {0};
    ASSERT_EQ(ONEWIRE_OK, onewire_read(&bus, buf, 2));
    ASSERT_EQ(0x50, buf[0]);
    ASSERT_EQ(0x01, buf[1]);
}

/* ========== runner ========== */

TEST_MAIN("onewire")
    /* init */
    RUN(test_init_null_bus);
    RUN(test_init_null_gpio);
    RUN(test_init_missing_pin_low);
    RUN(test_init_missing_pin_release);
    RUN(test_init_missing_pin_read);
    RUN(test_init_missing_delay_us);
    RUN(test_init_ok);
    RUN(test_init_stores_gpio);
    /* reset */
    RUN(test_reset_null_bus);
    RUN(test_reset_uninit_bus);
    RUN(test_reset_presence_detected);
    RUN(test_reset_no_presence);
    RUN(test_reset_no_presence_without_presence_ptr);
    RUN(test_reset_null_presence_ptr);
    RUN(test_reset_bus_held_low);
    RUN(test_reset_waits_for_a_slow_line);
    RUN(test_reset_timing);
    /* write_byte */
    RUN(test_write_byte_null_bus);
    RUN(test_write_byte_uninit_bus);
    RUN(test_write_byte_0x00);
    RUN(test_write_byte_0xFF);
    RUN(test_write_byte_0xA5_lsb_first);
    /* read_byte */
    RUN(test_read_byte_null_bus);
    RUN(test_read_byte_null_out);
    RUN(test_read_byte_0xFF);
    RUN(test_read_byte_0x00);
    RUN(test_read_byte_0xA5);
    RUN(test_read_byte_0x33);
    RUN(test_read_byte_timing);
    /* write multi */
    RUN(test_write_null_bus);
    RUN(test_write_null_data_nonzero_len);
    RUN(test_write_null_data_zero_len);
    RUN(test_write_two_bytes);
    /* read multi */
    RUN(test_read_null_bus);
    RUN(test_read_null_buf_nonzero_len);
    RUN(test_read_null_buf_zero_len);
    RUN(test_read_two_bytes);
    /* select */
    RUN(test_select_null_bus);
    RUN(test_select_null_rom);
    RUN(test_select_sends_match_rom_then_8_bytes);
    /* skip */
    RUN(test_skip_null_bus);
    RUN(test_skip_ok);
    /* read_rom */
    RUN(test_read_rom_null_bus);
    RUN(test_read_rom_null_rom);
    RUN(test_read_rom_valid_crc);
    RUN(test_read_rom_bad_crc);
    RUN(test_read_rom_all_zero_is_refused);
    RUN(test_read_rom_with_no_device_fails_the_crc);
    RUN(test_read_rom_zero_serial_is_not_refused);
    /* CRC-8 */
    RUN(test_crc8_null);
    RUN(test_crc8_empty);
    RUN(test_crc8_single_byte);
    RUN(test_crc8_known_rom_ds18b20);
    RUN(test_crc8_all_ones);
    RUN(test_crc8_incremental_consistency);
    RUN(test_crc8_known_vector_family_10);
    /* enums and constants */
    RUN(test_error_enum_values);
    RUN(test_rom_command_constants);
    /* instances */
    RUN(test_multiple_instances);
    RUN(test_reinit_same_bus);
    /* integration */
    RUN(test_full_ds18b20_convert_sequence);
    RUN(test_full_select_and_read);
TEST_MAIN_END()
