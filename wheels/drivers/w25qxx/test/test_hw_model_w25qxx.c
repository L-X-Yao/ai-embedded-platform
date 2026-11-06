// SPDX-License-Identifier: MIT
/*
 * test_hw_model_w25qxx.c -- tests using W25QXX hardware model
 *
 * Validates W25QXX driver against a flash-accurate model that
 * enforces NOR program semantics, write-enable requirements,
 * sector erase boundaries, JEDEC ID verification, the BUSY cycle,
 * /CS timing of program and erase, and the page wrap.
 */

#include "test_harness.h"
#include "hw_model_w25qxx.h"
#include "../w25qxx.h"

static w25qxx_model_t model;

static const w25qxx_spi_t model_spi = {
    .cs_low  = w25qxx_model_cs_low,
    .cs_high = w25qxx_model_cs_high,
    .xfer    = w25qxx_model_xfer,
    .ctx     = &model,
};

static void setup(void)
{
    w25qxx_model_init(&model);
}

/* Raw SPI, so that these tests do not lean on the driver. */
static void raw_begin(uint8_t cmd)
{
    w25qxx_model_cs_low(&model);
    (void)w25qxx_model_xfer(&model, cmd);
}

static void raw_end(void)
{
    w25qxx_model_cs_high(&model);
}

static void raw_cmd(uint8_t cmd)
{
    raw_begin(cmd);
    raw_end();
}

static void raw_addr(uint32_t addr)
{
    (void)w25qxx_model_xfer(&model, (uint8_t)(addr >> 16));
    (void)w25qxx_model_xfer(&model, (uint8_t)(addr >> 8));
    (void)w25qxx_model_xfer(&model, (uint8_t)addr);
}

static uint8_t raw_status(void)
{
    raw_begin(0x05);
    uint8_t sr = w25qxx_model_xfer(&model, 0xFF);
    raw_end();
    return sr;
}

static void raw_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    raw_begin(0x02);
    raw_addr(addr);
    for (uint16_t i = 0; i < len; i++)
        (void)w25qxx_model_xfer(&model, data[i]);
    raw_end();
}

/* Reads SR1 until BUSY goes, as much as a test could need */
static void raw_wait(void)
{
    for (int i = 0; i < 100 && (raw_status() & 0x01); i++) {
    }
}

/* --- init tests --- */

TEST(test_init_reads_jedec_id)
{
    setup();
    uint32_t id = 0;
    w25qxx_err_t err = w25qxx_init(&model_spi, &id);
    ASSERT_EQ(err, W25QXX_OK);
    ASSERT_EQ_HEX(id, 0xEF4017);
}

TEST(test_init_rejects_bad_jedec)
{
    setup();
    w25qxx_model_set_jedec(&model, 0xFF0000);
    uint32_t id = 0;
    ASSERT_EQ(w25qxx_init(&model_spi, &id), W25QXX_ERR_JEDEC_ID);
}

TEST(test_init_rejects_zero_manufacturer)
{
    setup();
    w25qxx_model_set_jedec(&model, 0x004017);
    uint32_t id = 0;
    ASSERT_EQ(w25qxx_init(&model_spi, &id), W25QXX_ERR_JEDEC_ID);
}

/* --- read tests --- */

TEST(test_read_data)
{
    setup();
    /* Pre-fill model memory */
    uint8_t fill[] = {0xDE, 0xAD, 0xBE, 0xEF};
    w25qxx_model_fill(&model, 0x100, fill, 4);

    uint8_t buf[4] = {0};
    w25qxx_err_t err = w25qxx_read(&model_spi, 0x100, buf, 4);
    ASSERT_EQ(err, W25QXX_OK);
    ASSERT_EQ_HEX(buf[0], 0xDE);
    ASSERT_EQ_HEX(buf[1], 0xAD);
    ASSERT_EQ_HEX(buf[2], 0xBE);
    ASSERT_EQ_HEX(buf[3], 0xEF);
}

TEST(test_read_erased_returns_ff)
{
    setup();
    uint8_t buf[4] = {0};
    w25qxx_read(&model_spi, 0x000, buf, 4);
    ASSERT_EQ_HEX(buf[0], 0xFF);
    ASSERT_EQ_HEX(buf[3], 0xFF);
}

/* --- page program tests --- */

TEST(test_page_program)
{
    setup();
    uint8_t data[] = {0x11, 0x22, 0x33};
    w25qxx_err_t err = w25qxx_page_program(&model_spi, 0x200, data, 3);
    ASSERT_EQ(err, W25QXX_OK);
    ASSERT_EQ(model.mem[0x200], 0x11);
    ASSERT_EQ(model.mem[0x201], 0x22);
    ASSERT_EQ(model.mem[0x202], 0x33);
    ASSERT_EQ(model.mem[0x203], 0xFF);
    ASSERT_TRUE(model.program_count >= 1);
}

TEST(test_nor_program_semantics)
{
    setup();
    /* Write 0x0F first */
    uint8_t d1 = 0x0F;
    w25qxx_page_program(&model_spi, 0x300, &d1, 1);
    ASSERT_EQ(model.mem[0x300], 0x0F);

    /* Write 0xF0 on top -- NOR can only clear bits */
    uint8_t d2 = 0xF0;
    w25qxx_page_program(&model_spi, 0x300, &d2, 1);
    ASSERT_EQ(model.mem[0x300], 0x00);
}

TEST(test_page_program_cross_boundary_rejected)
{
    setup();
    uint8_t data[2] = {0xAA, 0xBB};
    ASSERT_EQ(w25qxx_page_program(&model_spi, 0xFF, data, 2), W25QXX_ERR_ALIGN);
}

/* --- erase tests --- */

TEST(test_sector_erase)
{
    setup();
    model.mem[0x1000] = 0x42;
    model.mem[0x1FFF] = 0x42;
    model.mem[0x2000] = 0x42;  /* next sector, should survive */

    w25qxx_err_t err = w25qxx_sector_erase(&model_spi, 0x1000);
    ASSERT_EQ(err, W25QXX_OK);
    ASSERT_EQ(model.mem[0x1000], 0xFF);
    ASSERT_EQ(model.mem[0x1FFF], 0xFF);
    ASSERT_EQ(model.mem[0x2000], 0x42);
    ASSERT_TRUE(model.erase_count >= 1);
}

TEST(test_chip_erase)
{
    setup();
    model.mem[0] = 0x42;
    model.mem[W25QXX_MODEL_SIZE - 1] = 0x42;
    w25qxx_err_t err = w25qxx_chip_erase(&model_spi);
    ASSERT_EQ(err, W25QXX_OK);
    ASSERT_EQ(model.mem[0], 0xFF);
    ASSERT_EQ(model.mem[W25QXX_MODEL_SIZE - 1], 0xFF);
}

/* --- status register test --- */

TEST(test_read_status_not_busy)
{
    setup();
    uint8_t sr = 0xFF;
    w25qxx_err_t err = w25qxx_read_status(&model_spi, &sr);
    ASSERT_EQ(err, W25QXX_OK);
    ASSERT_EQ(sr & 0x01, 0);  /* not busy */
}

/* --- full write/read cycle --- */

TEST(test_erase_program_read_cycle)
{
    setup();
    /* Erase sector 0 */
    w25qxx_sector_erase(&model_spi, 0);

    /* Program */
    uint8_t wr[8] = {0xCA, 0xFE, 0xBA, 0xBE, 0xDE, 0xAD, 0xBE, 0xEF};
    w25qxx_page_program(&model_spi, 0, wr, 8);

    /* Read back */
    uint8_t rd[8] = {0};
    w25qxx_read(&model_spi, 0, rd, 8);
    ASSERT_TRUE(memcmp(wr, rd, 8) == 0);
}

/* --- what the model itself must do --- */

/* p.36: "The BUSY bit is a 1 during the Page Program cycle and becomes a 0 when
 * the cycle is finished", and "After the Page Program cycle has finished the
 * Write Enable Latch (WEL) bit in the Status Register is cleared to 0." */
TEST(test_model_busy_lasts_for_the_program_cycle)
{
    setup();
    w25qxx_model_set_busy_reads(&model, 3);
    const uint8_t data[1] = { 0x12 };

    raw_cmd(0x06);
    ASSERT_EQ_HEX(raw_status(), 0x02);
    raw_program(0x100, data, 1);

    ASSERT_EQ_HEX(raw_status(), 0x03);
    ASSERT_EQ_HEX(raw_status(), 0x03);
    ASSERT_EQ_HEX(raw_status(), 0x03);
    ASSERT_EQ_HEX(raw_status(), 0x00);
    ASSERT_EQ_HEX(model.mem[0x100], 0x12);
}

/* p.22: "all instructions except for Read Status Register will be ignored" */
TEST(test_model_ignores_all_but_read_status_while_busy)
{
    setup();
    w25qxx_model_set_busy_reads(&model, 5);
    const uint8_t first[1] = { 0x12 };
    const uint8_t second[1] = { 0x34 };

    raw_cmd(0x06);
    raw_program(0x100, first, 1);

    /* a read of the JEDEC ID gets nothing */
    raw_begin(0x9F);
    ASSERT_EQ_HEX(w25qxx_model_xfer(&model, 0xFF), 0xFF);
    raw_end();

    /* nor do Write Enable, or a program on top */
    raw_cmd(0x06);
    raw_program(0x101, second, 1);
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0x100], 0x12);
    ASSERT_EQ_HEX(model.mem[0x101], 0xFF);
    ASSERT_EQ(model.program_count, 1);
    ASSERT_EQ_HEX(raw_status(), 0x00);
}

/* p.36: no WEL, no Page Program. */
TEST(test_model_needs_write_enable_for_each_program)
{
    setup();
    const uint8_t data[1] = { 0x12 };
    raw_program(0x100, data, 1);
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0x100], 0xFF);

    raw_cmd(0x06);
    raw_program(0x100, data, 1);
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0x100], 0x12);

    raw_program(0x101, data, 1);
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0x101], 0xFF);
}

/* p.36: the instruction runs once /CS has been driven high. */
TEST(test_model_programs_only_when_cs_rises)
{
    setup();
    raw_cmd(0x06);
    raw_begin(0x02);
    raw_addr(0x100);
    (void)w25qxx_model_xfer(&model, 0x12);
    ASSERT_EQ_HEX(model.mem[0x100], 0xFF);
    ASSERT_EQ(model.program_count, 0);
    raw_end();
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0x100], 0x12);
    ASSERT_EQ(model.program_count, 1);
}

/* p.41: "The /CS pin must be driven high after the eighth bit has been latched.
 * If this is not done the Chip Erase instruction will not be executed." */
TEST(test_model_chip_erase_runs_only_on_cs_high_after_8_bits)
{
    setup();
    model.mem[0] = 0x42;

    raw_cmd(0x06);
    raw_begin(0xC7);
    ASSERT_EQ_HEX(model.mem[0], 0x42);
    (void)w25qxx_model_xfer(&model, 0x00);
    raw_end();
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0], 0x42);
    ASSERT_EQ(model.erase_count, 0);

    raw_cmd(0xC7);
    raw_wait();
    ASSERT_EQ_HEX(model.mem[0], 0xFF);
    ASSERT_EQ(model.erase_count, 1);
}

/* p.36: "If the last address byte is not zero, and the number of clocks exceeds
 * the remaining page length, the addressing will wrap to the beginning of the
 * page." */
TEST(test_model_page_program_wraps_inside_the_page)
{
    setup();
    const uint8_t data[4] = { 0x11, 0x22, 0x33, 0x44 };
    raw_cmd(0x06);
    raw_program(0x1FE, data, 4);
    raw_wait();

    ASSERT_EQ_HEX(model.mem[0x1FE], 0x11);
    ASSERT_EQ_HEX(model.mem[0x1FF], 0x22);
    ASSERT_EQ_HEX(model.mem[0x100], 0x33);
    ASSERT_EQ_HEX(model.mem[0x101], 0x44);
    ASSERT_EQ_HEX(model.mem[0x200], 0xFF);
}

/* p.36: "If more than 256 bytes are sent to the device the addressing will wrap
 * to the beginning of the page and overwrite previously sent data." */
TEST(test_model_more_than_a_page_overwrites)
{
    setup();
    uint8_t data[257];
    memset(data, 0x7F, sizeof(data));
    data[256] = 0x0F;
    raw_cmd(0x06);
    raw_program(0x100, data, 257);
    raw_wait();

    ASSERT_EQ_HEX(model.mem[0x100], 0x0F);
    ASSERT_EQ_HEX(model.mem[0x101], 0x7F);
}

/* --- the driver against the model --- */

TEST(test_driver_waits_out_a_long_cycle)
{
    setup();
    w25qxx_model_set_busy_reads(&model, 1000);

    const uint8_t data[3] = { 0x11, 0x22, 0x33 };
    ASSERT_EQ(w25qxx_page_program(&model_spi, 0x200, data, 3), W25QXX_OK);
    ASSERT_EQ(model.program_count, 1);

    ASSERT_EQ(w25qxx_sector_erase(&model_spi, 0x200), W25QXX_OK);
    ASSERT_EQ_HEX(model.mem[0x200], 0xFF);
    ASSERT_EQ(model.erase_count, 1);
}

TEST(test_driver_programs_page_after_page)
{
    setup();
    w25qxx_model_set_busy_reads(&model, 10);

    uint8_t wr[256];
    for (unsigned i = 0; i < 256u; i++)
        wr[i] = (uint8_t)i;
    ASSERT_EQ(w25qxx_page_program(&model_spi, 0x100, wr, 256), W25QXX_OK);
    wr[0] = 0xAB;
    ASSERT_EQ(w25qxx_page_program(&model_spi, 0x200, wr, 256), W25QXX_OK);

    uint8_t rd[256] = {0};
    ASSERT_EQ(w25qxx_read(&model_spi, 0x200, rd, 256), W25QXX_OK);
    ASSERT_MEMEQ_N(rd, wr, 256);
    ASSERT_EQ_HEX(model.mem[0x100], 0x00);
    ASSERT_EQ(model.program_count, 2);
}

/* The chip clears WEL after every cycle, so the driver must set it each time. */
TEST(test_driver_leaves_wel_clear)
{
    setup();
    const uint8_t data[1] = { 0x12 };
    ASSERT_EQ(w25qxx_page_program(&model_spi, 0x100, data, 1), W25QXX_OK);

    uint8_t sr = 0xFF;
    ASSERT_EQ(w25qxx_read_status(&model_spi, &sr), W25QXX_OK);
    ASSERT_EQ_HEX(sr, 0x00);
}

/* BUSY that never ends. The driver's poll limit is its own. */
TEST(test_driver_gives_up_on_a_stuck_chip)
{
    setup();
    w25qxx_model_set_busy_reads(&model, 0xFFFFFFFFu);
    const uint8_t data[1] = { 0x12 };
    ASSERT_EQ(w25qxx_page_program(&model_spi, 0x100, data, 1), W25QXX_ERR_TIMEOUT);
}

TEST(test_chip_erase_is_counted)
{
    setup();
    ASSERT_EQ(w25qxx_chip_erase(&model_spi), W25QXX_OK);
    ASSERT_EQ(model.erase_count, 1);
}

TEST_MAIN("w25qxx hardware model")
    RUN(test_init_reads_jedec_id);
    RUN(test_init_rejects_bad_jedec);
    RUN(test_init_rejects_zero_manufacturer);
    RUN(test_read_data);
    RUN(test_read_erased_returns_ff);
    RUN(test_page_program);
    RUN(test_nor_program_semantics);
    RUN(test_page_program_cross_boundary_rejected);
    RUN(test_sector_erase);
    RUN(test_chip_erase);
    RUN(test_read_status_not_busy);
    RUN(test_erase_program_read_cycle);
    RUN(test_model_busy_lasts_for_the_program_cycle);
    RUN(test_model_ignores_all_but_read_status_while_busy);
    RUN(test_model_needs_write_enable_for_each_program);
    RUN(test_model_programs_only_when_cs_rises);
    RUN(test_model_chip_erase_runs_only_on_cs_high_after_8_bits);
    RUN(test_model_page_program_wraps_inside_the_page);
    RUN(test_model_more_than_a_page_overwrites);
    RUN(test_driver_waits_out_a_long_cycle);
    RUN(test_driver_programs_page_after_page);
    RUN(test_driver_leaves_wel_clear);
    RUN(test_driver_gives_up_on_a_stuck_chip);
    RUN(test_chip_erase_is_counted);
TEST_MAIN_END()
