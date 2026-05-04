/*
 * test_hw_model_at24cxx.c -- tests using AT24CXX EEPROM hardware model
 *
 * Validates AT24CXX driver against an EEPROM model (AT24C02, 256 bytes).
 * Tests init, single-byte write, multi-byte write with page splitting,
 * read-back, boundary checks, and write-cycle delays.
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

static void setup(void)
{
    at24cxx_hw_model_init(&model);
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
TEST_MAIN_END()
