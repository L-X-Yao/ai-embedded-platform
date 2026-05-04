/*
 * test_hw_model_w25qxx.c -- tests using W25QXX hardware model
 *
 * Validates W25QXX driver against a flash-accurate model that
 * enforces NOR program semantics, write-enable requirements,
 * sector erase boundaries, and JEDEC ID verification.
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
TEST_MAIN_END()
