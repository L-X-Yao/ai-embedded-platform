// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "mcp4725.h"

/* ---------- MCP4725-aware mock I2C ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static uint16_t mock_dac_value;
static uint8_t  mock_dac_pd;
static uint16_t mock_eeprom_value;
static uint8_t  mock_eeprom_pd;
static bool     mock_eeprom_busy;

static uint8_t  mock_last_write[8];
static int      mock_last_write_len;
static int      mock_write_count;
static int      mock_read_count;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    mock_dac_value  = 0;
    mock_dac_pd     = 0;
    mock_eeprom_value = 0;
    mock_eeprom_pd  = 0;
    mock_eeprom_busy = false;
    memset(mock_last_write, 0, sizeof(mock_last_write));
    mock_last_write_len = 0;
    mock_write_count = 0;
    mock_read_count  = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_write_count++;
    if (len <= (uint16_t)sizeof(mock_last_write)) {
        memcpy(mock_last_write, data, len);
        mock_last_write_len = (int)len;
    }

    if (len == 2) {
        /* Fast mode write */
        mock_dac_pd    = (data[0] >> 4) & 0x03u;
        mock_dac_value = (uint16_t)(((uint16_t)(data[0] & 0x0Fu) << 8) |
                                    (uint16_t)data[1]);
    } else if (len == 3) {
        uint8_t cmd = data[0] & 0xE0u;
        uint8_t pd  = (data[0] >> 1) & 0x03u;
        uint16_t val = (uint16_t)(((uint16_t)data[1] << 4) |
                                  ((uint16_t)data[2] >> 4));

        if (cmd == 0x40u) {
            mock_dac_value = val;
            mock_dac_pd    = pd;
        } else if (cmd == 0x60u) {
            mock_dac_value    = val;
            mock_dac_pd       = pd;
            mock_eeprom_value = val;
            mock_eeprom_pd    = pd;
        }
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_read_count++;
    if (len >= 5) {
        uint8_t rdy = mock_eeprom_busy ? 0x00u : 0x80u;
        data[0] = (uint8_t)(rdy | ((unsigned)mock_dac_pd << 2) |
                             ((mock_dac_value >> 10) & 0x03u));
        data[1] = (uint8_t)((mock_dac_value >> 2) & 0xFFu);
        data[2] = (uint8_t)((mock_dac_value & 0x03u) << 6);
        data[3] = (uint8_t)((mock_eeprom_pd << 5) |
                             ((mock_eeprom_value >> 8) & 0x0Fu));
        data[4] = (uint8_t)(mock_eeprom_value & 0xFFu);
    }
    return true;
}

static const mcp4725_i2c_t mock_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

static mcp4725_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A), MCP4725_OK);
    ASSERT_EQ(mock_read_count, 1);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A), MCP4725_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(mcp4725_init(NULL, &mock_i2c, 0x60), MCP4725_ERR_PARAM);
    ASSERT_EQ(mcp4725_init(&dev, NULL, 0x60), MCP4725_ERR_PARAM);
    ASSERT_EQ(mcp4725_write_fast(NULL, 0), MCP4725_ERR_PARAM);
    ASSERT_EQ(mcp4725_write_dac(NULL, 0, MCP4725_PD_NORMAL), MCP4725_ERR_PARAM);
    ASSERT_EQ(mcp4725_write_eeprom(NULL, 0, MCP4725_PD_NORMAL), MCP4725_ERR_PARAM);
    mcp4725_status_t st;
    ASSERT_EQ(mcp4725_read_status(NULL, &st), MCP4725_ERR_PARAM);
    ASSERT_EQ(mcp4725_read_status(&dev, NULL), MCP4725_ERR_PARAM);
}

TEST(test_write_fast_midscale)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_fast(&dev, 2048), MCP4725_OK);
    ASSERT_EQ(mock_dac_value, 2048);
    ASSERT_EQ(mock_last_write_len, 2);
    ASSERT_EQ(mock_last_write[0], 0x08);
    ASSERT_EQ(mock_last_write[1], 0x00);
}

TEST(test_write_fast_max)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_fast(&dev, 4095), MCP4725_OK);
    ASSERT_EQ(mock_dac_value, 4095);
    ASSERT_EQ(mock_last_write[0], 0x0F);
    ASSERT_EQ(mock_last_write[1], 0xFF);
}

TEST(test_write_fast_zero)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_fast(&dev, 0), MCP4725_OK);
    ASSERT_EQ(mock_dac_value, 0);
}

TEST(test_write_fast_over_max)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_fast(&dev, 4096), MCP4725_ERR_PARAM);
    ASSERT_EQ(mcp4725_write_fast(&dev, 0xFFFF), MCP4725_ERR_PARAM);
}

TEST(test_write_fast_i2c_fail)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    mock_fail_next = 1;
    ASSERT_EQ(mcp4725_write_fast(&dev, 1000), MCP4725_ERR_I2C);
}

TEST(test_write_dac_normal)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_dac(&dev, 3000, MCP4725_PD_NORMAL), MCP4725_OK);
    ASSERT_EQ(mock_dac_value, 3000);
    ASSERT_EQ(mock_dac_pd, 0);
    ASSERT_EQ(mock_last_write_len, 3);
    ASSERT_EQ(mock_last_write[0], 0x40);
}

TEST(test_write_dac_power_down_1k)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_dac(&dev, 0, MCP4725_PD_1K), MCP4725_OK);
    ASSERT_EQ(mock_dac_pd, 1);
    ASSERT_EQ(mock_last_write[0], 0x42);
}

TEST(test_write_dac_power_down_500k)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_dac(&dev, 0, MCP4725_PD_500K), MCP4725_OK);
    ASSERT_EQ(mock_dac_pd, 3);
    ASSERT_EQ(mock_last_write[0], 0x46);
}

/* DS22039D Figure 6-2: 2nd byte is C2 C1 C0 X X PD1 PD0 X, with PD1 PD0 from
 * Table 5-2: 00 normal, 01 1 kOhm, 10 100 kOhm, 11 500 kOhm. */
TEST(test_write_command_byte_all_pd_modes)
{
    static const struct { mcp4725_pd_t pd; uint8_t dac; uint8_t eeprom; } rows[] = {
        { MCP4725_PD_NORMAL, 0x40u, 0x60u },  /* 010 00 00 0, 011 00 00 0 */
        { MCP4725_PD_1K,     0x42u, 0x62u },  /* 010 00 01 0, 011 00 01 0 */
        { MCP4725_PD_100K,   0x44u, 0x64u },  /* 010 00 10 0, 011 00 10 0 */
        { MCP4725_PD_500K,   0x46u, 0x66u },  /* 010 00 11 0, 011 00 11 0 */
    };

    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ASSERT_EQ(mcp4725_write_dac(&dev, 0, rows[i].pd), MCP4725_OK);
        ASSERT_EQ_HEX(mock_last_write[0], rows[i].dac);
        ASSERT_EQ(mcp4725_write_eeprom(&dev, 0, rows[i].pd), MCP4725_OK);
        ASSERT_EQ_HEX(mock_last_write[0], rows[i].eeprom);
    }
}

TEST(test_write_dac_i2c_fail)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    mock_fail_next = 1;
    ASSERT_EQ(mcp4725_write_dac(&dev, 100, MCP4725_PD_NORMAL), MCP4725_ERR_I2C);
}

TEST(test_write_dac_over_max)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    ASSERT_EQ(mcp4725_write_dac(&dev, 4096, MCP4725_PD_NORMAL), MCP4725_ERR_PARAM);
}

TEST(test_write_eeprom)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_eeprom(&dev, 2500, MCP4725_PD_NORMAL), MCP4725_OK);
    ASSERT_EQ(mock_dac_value, 2500);
    ASSERT_EQ(mock_eeprom_value, 2500);
    ASSERT_EQ(mock_last_write[0], 0x60);
}

TEST(test_write_eeprom_with_pd)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    ASSERT_EQ(mcp4725_write_eeprom(&dev, 1000, MCP4725_PD_100K), MCP4725_OK);
    ASSERT_EQ(mock_eeprom_pd, 2);
    ASSERT_EQ(mock_last_write[0], 0x64);
}

TEST(test_write_eeprom_i2c_fail)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    mock_fail_next = 1;
    ASSERT_EQ(mcp4725_write_eeprom(&dev, 100, MCP4725_PD_NORMAL), MCP4725_ERR_I2C);
}

TEST(test_write_eeprom_over_max)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    ASSERT_EQ(mcp4725_write_eeprom(&dev, 5000, MCP4725_PD_NORMAL), MCP4725_ERR_PARAM);
}

TEST(test_read_status_roundtrip)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    mcp4725_write_dac(&dev, 3000, MCP4725_PD_NORMAL);
    mcp4725_write_eeprom(&dev, 1500, MCP4725_PD_100K);

    mcp4725_status_t st;
    ASSERT_EQ(mcp4725_read_status(&dev, &st), MCP4725_OK);
    ASSERT_EQ(st.dac_value, 1500);
    ASSERT_EQ(st.pd_mode, 2);
    ASSERT_EQ(st.eeprom_value, 1500);
    ASSERT_EQ(st.eeprom_pd, 2);
    ASSERT_EQ(st.eeprom_busy, false);
}

TEST(test_read_status_busy)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    mock_eeprom_busy = true;

    mcp4725_status_t st;
    ASSERT_EQ(mcp4725_read_status(&dev, &st), MCP4725_OK);
    ASSERT_EQ(st.eeprom_busy, true);
}

TEST(test_read_status_i2c_fail)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);
    mock_fail_next = 1;
    mcp4725_status_t st;
    ASSERT_EQ(mcp4725_read_status(&dev, &st), MCP4725_ERR_I2C);
}

TEST(test_write_dac_value_encoding)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    /* Value 0x9C4 = 2500: byte1 = 0x9C, byte2 = 0x40 */
    mcp4725_write_dac(&dev, 2500, MCP4725_PD_NORMAL);
    ASSERT_EQ(mock_last_write[1], 0x9C);
    ASSERT_EQ(mock_last_write[2], 0x40);
}

TEST(test_read_status_max_values)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    mcp4725_write_eeprom(&dev, 4095, MCP4725_PD_500K);

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.dac_value, 4095);
    ASSERT_EQ(st.pd_mode, 3);
    ASSERT_EQ(st.eeprom_value, 4095);
    ASSERT_EQ(st.eeprom_pd, 3);
}

TEST(test_addr_b)
{
    mock_reset();
    mcp4725_t d;
    ASSERT_EQ(mcp4725_init(&d, &mock_i2c, MCP4725_ADDR_B), MCP4725_OK);
}

TEST(test_fast_does_not_touch_eeprom)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    mcp4725_write_eeprom(&dev, 1000, MCP4725_PD_NORMAL);
    mcp4725_write_fast(&dev, 3000);

    ASSERT_EQ(mock_dac_value, 3000);
    ASSERT_EQ(mock_eeprom_value, 1000);
}

TEST(test_dac_does_not_touch_eeprom)
{
    mock_reset();
    mcp4725_init(&dev, &mock_i2c, MCP4725_ADDR_A);

    mcp4725_write_eeprom(&dev, 500, MCP4725_PD_NORMAL);
    mcp4725_write_dac(&dev, 2000, MCP4725_PD_1K);

    ASSERT_EQ(mock_dac_value, 2000);
    ASSERT_EQ(mock_dac_pd, 1);
    ASSERT_EQ(mock_eeprom_value, 500);
    ASSERT_EQ(mock_eeprom_pd, 0);
}

TEST_MAIN("mcp4725")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_write_fast_midscale);
    RUN(test_write_fast_max);
    RUN(test_write_fast_zero);
    RUN(test_write_fast_over_max);
    RUN(test_write_fast_i2c_fail);
    RUN(test_write_dac_normal);
    RUN(test_write_dac_power_down_1k);
    RUN(test_write_dac_power_down_500k);
    RUN(test_write_command_byte_all_pd_modes);
    RUN(test_write_dac_i2c_fail);
    RUN(test_write_dac_over_max);
    RUN(test_write_eeprom);
    RUN(test_write_eeprom_with_pd);
    RUN(test_write_eeprom_i2c_fail);
    RUN(test_write_eeprom_over_max);
    RUN(test_read_status_roundtrip);
    RUN(test_read_status_busy);
    RUN(test_read_status_i2c_fail);
    RUN(test_write_dac_value_encoding);
    RUN(test_read_status_max_values);
    RUN(test_addr_b);
    RUN(test_fast_does_not_touch_eeprom);
    RUN(test_dac_does_not_touch_eeprom);
TEST_MAIN_END()
