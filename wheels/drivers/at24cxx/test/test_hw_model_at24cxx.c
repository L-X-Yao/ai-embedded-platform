// SPDX-License-Identifier: MIT
/*
 * test_hw_model_at24cxx.c -- tests using AT24CXX EEPROM hardware model
 *
 * Validates AT24CXX driver against an EEPROM model: the AT24C02, and the
 * AT24C256 of the manual. Tests init, single-byte write, multi-byte write with
 * page splitting, read-back, boundary checks, and the write cycle.
 */

#include "test_harness.h"
#include "hw_model_at24cxx.h"
#include "../at24cxx.h"

static at24cxx_hw_model_t model;

static const at24cxx_i2c_t model_i2c = {
    .write_reg   = at24cxx_model_i2c_write_reg,
    .read_regs   = at24cxx_model_i2c_read_regs,
    .write_regs8 = at24cxx_model_i2c_write_regs8,
    .write_regs  = NULL,       /* not needed for 8-bit address chips */
    .read_regs16 = NULL,       /* not needed for 8-bit address chips */
    .delay_ms    = at24cxx_model_delay,
    .ctx         = &model,
};

/* No write_regs8: the driver falls back to one write_reg per byte. */
static const at24cxx_i2c_t model_i2c_bytes = {
    .write_reg   = at24cxx_model_i2c_write_reg,
    .read_regs   = at24cxx_model_i2c_read_regs,
    .delay_ms    = at24cxx_model_delay,
    .ctx         = &model,
};

/* The 16-bit word addresses of the AT24C128/256 */
static const at24cxx_i2c_t model_i2c_16 = {
    .write_regs  = at24cxx_model_i2c_write_regs,
    .read_regs16 = at24cxx_model_i2c_read_regs16,
    .delay_ms    = at24cxx_model_delay,
    .ctx         = &model,
};

static void setup(void)
{
    at24cxx_hw_model_init(&model);
}

static void setup_c256(void)
{
    at24cxx_hw_model_init_part(&model, 32768u, 64u, 0x50);
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    at24cxx_t dev;
    at24cxx_err_t err = at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);
    ASSERT_EQ(err, AT24CXX_OK);
}

TEST(test_init_rejects_null)
{
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, NULL, 0x50, AT24CXX_C02), AT24CXX_ERR_PARAM);
}

/* --- write/read single byte --- */

TEST(test_write_byte_read_back)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    at24cxx_err_t err = at24cxx_write_byte(&dev, 0x10, 0xAB);
    ASSERT_EQ(err, AT24CXX_OK);

    uint8_t val = 0;
    err = at24cxx_read(&dev, 0x10, &val, 1);
    ASSERT_EQ(err, AT24CXX_OK);
    ASSERT_EQ_HEX(val, 0xAB);
}

/* --- multi-byte write/read --- */

TEST(test_write_read_multi)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    uint8_t wr[] = {0xDE, 0xAD, 0xBE, 0xEF};
    at24cxx_err_t err = at24cxx_write(&dev, 0x20, wr, 4);
    ASSERT_EQ(err, AT24CXX_OK);

    uint8_t rd[4] = {0};
    err = at24cxx_read(&dev, 0x20, rd, 4);
    ASSERT_EQ(err, AT24CXX_OK);
    ASSERT_EQ_HEX(rd[0], 0xDE);
    ASSERT_EQ_HEX(rd[1], 0xAD);
    ASSERT_EQ_HEX(rd[2], 0xBE);
    ASSERT_EQ_HEX(rd[3], 0xEF);
}

/* --- page boundary splitting --- */

TEST(test_write_crosses_page_boundary)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    /* AT24C02 page size = 8. Write 4 bytes starting at addr 6 (crosses page 0->1). */
    uint8_t wr[] = {0x11, 0x22, 0x33, 0x44};
    at24cxx_err_t err = at24cxx_write(&dev, 6, wr, 4);
    ASSERT_EQ(err, AT24CXX_OK);

    uint8_t rd[4] = {0};
    at24cxx_read(&dev, 6, rd, 4);
    ASSERT_EQ_HEX(rd[0], 0x11);
    ASSERT_EQ_HEX(rd[1], 0x22);
    ASSERT_EQ_HEX(rd[2], 0x33);
    ASSERT_EQ_HEX(rd[3], 0x44);

    /* Should have inserted write-cycle delays (at least 2 pages) */
    ASSERT_TRUE(model.delay_count >= 2);
}

/* --- erased state --- */

TEST(test_erased_reads_ff)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    uint8_t buf[4] = {0};
    at24cxx_read(&dev, 0, buf, 4);
    ASSERT_EQ_HEX(buf[0], 0xFF);
    ASSERT_EQ_HEX(buf[3], 0xFF);
}

/* --- address boundary --- */

TEST(test_read_beyond_size)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    /* AT24C02 = 256 bytes. Read at addr 255, len 2 -> out of bounds */
    uint8_t buf[2];
    ASSERT_EQ(at24cxx_read(&dev, 255, buf, 2), AT24CXX_ERR_ADDR);
}

TEST(test_write_beyond_size)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    uint8_t data = 0x42;
    ASSERT_EQ(at24cxx_write(&dev, 256, &data, 1), AT24CXX_ERR_ADDR);
}

/* --- pre-filled data readback --- */

TEST(test_prefilled_read)
{
    setup();
    uint8_t fill[] = {0xCA, 0xFE};
    at24cxx_model_fill(&model, 0x80, fill, 2);

    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    uint8_t buf[2] = {0};
    at24cxx_read(&dev, 0x80, buf, 2);
    ASSERT_EQ_HEX(buf[0], 0xCA);
    ASSERT_EQ_HEX(buf[1], 0xFE);
}

/* --- overwrite --- */

TEST(test_overwrite)
{
    setup();
    at24cxx_t dev;
    at24cxx_init(&dev, &model_i2c, 0x50, AT24CXX_C02);

    at24cxx_write_byte(&dev, 0x00, 0xAA);
    at24cxx_write_byte(&dev, 0x00, 0x55);

    uint8_t val = 0;
    at24cxx_read(&dev, 0x00, &val, 1);
    ASSERT_EQ_HEX(val, 0x55);  /* EEPROM can overwrite freely */
}

/* --- what the model itself must do --- */

/* p.10: "When the word address, internally generated, reaches the page
 * boundary, the following byte is placed at the beginning of the same page." */
TEST(test_model_page_write_wraps_inside_the_page)
{
    setup_c256();
    const uint8_t data[4] = { 0x11, 0x22, 0x33, 0x44 };
    ASSERT_TRUE(at24cxx_model_i2c_write_regs(&model, 0x50, 62, data, 4));
    at24cxx_model_delay(&model, 20);

    uint8_t back[2];
    ASSERT_TRUE(at24cxx_model_i2c_read_regs16(&model, 0x50, 62, back, 2));
    ASSERT_EQ_HEX(back[0], 0x11);
    ASSERT_EQ_HEX(back[1], 0x22);
    ASSERT_TRUE(at24cxx_model_i2c_read_regs16(&model, 0x50, 0, back, 2));
    ASSERT_EQ_HEX(back[0], 0x33);
    ASSERT_EQ_HEX(back[1], 0x44);
    ASSERT_TRUE(at24cxx_model_i2c_read_regs16(&model, 0x50, 64, back, 1));
    ASSERT_EQ_HEX(back[0], 0xFF);
}

/* The 8-byte page of the AT24C02, through the page write callback. */
TEST(test_model_c02_page_write_wraps_inside_the_page)
{
    setup();
    const uint8_t data[4] = { 0x11, 0x22, 0x33, 0x44 };
    ASSERT_TRUE(at24cxx_model_i2c_write_regs8(&model, 0x50, 6, data, 4));
    at24cxx_model_delay(&model, 20);

    uint8_t back[2];
    ASSERT_TRUE(at24cxx_model_i2c_read_regs(&model, 0x50, 0, back, 2));
    ASSERT_EQ_HEX(back[0], 0x33);
    ASSERT_EQ_HEX(back[1], 0x44);
    ASSERT_TRUE(at24cxx_model_i2c_read_regs(&model, 0x50, 8, back, 1));
    ASSERT_EQ_HEX(back[0], 0xFF);
}

/* p.10: "All inputs are disabled during this write cycle and the EEPROM will
 * not respond until the write is complete". p.5: tWR is 20 ms at 1.8 V. */
TEST(test_model_does_not_respond_during_the_write_cycle)
{
    setup_c256();
    const uint8_t data[1] = { 0x5A };
    uint8_t back = 0;
    ASSERT_TRUE(at24cxx_model_i2c_write_regs(&model, 0x50, 0, data, 1));
    ASSERT_FALSE(at24cxx_model_i2c_read_regs16(&model, 0x50, 0, &back, 1));
    ASSERT_FALSE(at24cxx_model_i2c_write_regs(&model, 0x50, 1, data, 1));

    at24cxx_model_delay(&model, 19);
    ASSERT_FALSE(at24cxx_model_i2c_read_regs16(&model, 0x50, 0, &back, 1));

    at24cxx_model_delay(&model, 1);
    ASSERT_TRUE(at24cxx_model_i2c_read_regs16(&model, 0x50, 0, &back, 1));
    ASSERT_EQ_HEX(back, 0x5A);
}

/* p.11: "The address "roll over" during read is from the last byte of the last
 * memory page, to the first byte of the first page." */
TEST(test_model_read_rolls_over_at_the_end_of_memory)
{
    setup_c256();
    const uint8_t first[1] = { 0xA1 };
    const uint8_t last[1]  = { 0xB2 };
    ASSERT_TRUE(at24cxx_model_i2c_write_regs(&model, 0x50, 0, first, 1));
    at24cxx_model_delay(&model, 20);
    ASSERT_TRUE(at24cxx_model_i2c_write_regs(&model, 0x50, 32767, last, 1));
    at24cxx_model_delay(&model, 20);

    uint8_t back[2];
    ASSERT_TRUE(at24cxx_model_i2c_read_regs16(&model, 0x50, 32767, back, 2));
    ASSERT_EQ_HEX(back[0], 0xB2);
    ASSERT_EQ_HEX(back[1], 0xA1);
}

/* p.10: "Upon a compare of the device address, the EEPROM will output a zero.
 * If a compare is not made, the device will return to a standby state." */
TEST(test_model_answers_at_its_address_only)
{
    setup_c256();
    const uint8_t data[1] = { 0x5A };
    uint8_t back = 0;
    ASSERT_FALSE(at24cxx_model_i2c_write_regs(&model, 0x51, 0, data, 1));
    ASSERT_FALSE(at24cxx_model_i2c_read_regs16(&model, 0x51, 0, &back, 1));
    ASSERT_TRUE(at24cxx_model_i2c_read_regs16(&model, 0x50, 0, &back, 1));
    ASSERT_EQ_HEX(back, 0xFF);
}

/* --- the driver against the model --- */

TEST(test_wrong_device_address_is_an_i2c_error)
{
    setup_c256();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &model_i2c_16, 0x52, AT24CXX_C256), AT24CXX_OK);
    uint8_t buf[1];
    ASSERT_EQ(at24cxx_read(&dev, 0, buf, 1), AT24CXX_ERR_I2C);
    ASSERT_EQ(at24cxx_write_byte(&dev, 0, 0x11), AT24CXX_ERR_I2C);
}

/* Three pages and a bit: every page must end before its boundary and every
 * write cycle must have ended before the next transfer. */
TEST(test_c256_long_write_reads_back)
{
    setup_c256();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &model_i2c_16, 0x50, AT24CXX_C256), AT24CXX_OK);

    uint8_t wr[200];
    for (unsigned i = 0; i < sizeof(wr); i++)
        wr[i] = (uint8_t)(i + 1u);
    ASSERT_EQ(at24cxx_write(&dev, 50, wr, sizeof(wr)), AT24CXX_OK);

    uint8_t rd[200] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 50, rd, sizeof(rd)), AT24CXX_OK);
    ASSERT_MEMEQ_N(rd, wr, sizeof(wr));
    ASSERT_EQ(model.write_count, 4);
}

TEST(test_c256_last_byte)
{
    setup_c256();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &model_i2c_16, 0x50, AT24CXX_C256), AT24CXX_OK);
    ASSERT_EQ(at24cxx_write_byte(&dev, 32767, 0x7E), AT24CXX_OK);

    uint8_t val = 0;
    ASSERT_EQ(at24cxx_read(&dev, 32767, &val, 1), AT24CXX_OK);
    ASSERT_EQ_HEX(val, 0x7E);
}

/* With one write_reg per byte, each STOP starts a write cycle of its own. */
TEST(test_write_byte_by_byte_waits_for_each_write_cycle)
{
    setup();
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &model_i2c_bytes, 0x50, AT24CXX_C02), AT24CXX_OK);

    uint8_t wr[] = { 0x11, 0x22, 0x33, 0x44 };
    ASSERT_EQ(at24cxx_write(&dev, 6, wr, 4), AT24CXX_OK);

    uint8_t rd[4] = {0};
    ASSERT_EQ(at24cxx_read(&dev, 6, rd, 4), AT24CXX_OK);
    ASSERT_MEMEQ_N(rd, wr, 4);
}

/* The chip gives no ACK until tWR has passed, and only delay_ms can wait. */
TEST(test_init_refuses_no_delay)
{
    setup();
    at24cxx_i2c_t hal = model_i2c;
    hal.delay_ms = NULL;
    at24cxx_t dev;
    ASSERT_EQ(at24cxx_init(&dev, &hal, 0x50, AT24CXX_C02), AT24CXX_ERR_PARAM);
}

TEST_MAIN("at24cxx hardware model")
    RUN(test_init_success);
    RUN(test_init_rejects_null);
    RUN(test_write_byte_read_back);
    RUN(test_write_read_multi);
    RUN(test_write_crosses_page_boundary);
    RUN(test_erased_reads_ff);
    RUN(test_read_beyond_size);
    RUN(test_write_beyond_size);
    RUN(test_prefilled_read);
    RUN(test_overwrite);
    RUN(test_model_page_write_wraps_inside_the_page);
    RUN(test_model_c02_page_write_wraps_inside_the_page);
    RUN(test_model_does_not_respond_during_the_write_cycle);
    RUN(test_model_read_rolls_over_at_the_end_of_memory);
    RUN(test_model_answers_at_its_address_only);
    RUN(test_wrong_device_address_is_an_i2c_error);
    RUN(test_c256_long_write_reads_back);
    RUN(test_c256_last_byte);
    RUN(test_write_byte_by_byte_waits_for_each_write_cycle);
    RUN(test_init_refuses_no_delay);
TEST_MAIN_END()
