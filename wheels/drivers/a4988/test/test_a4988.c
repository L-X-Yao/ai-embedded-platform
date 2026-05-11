#include "test_harness.h"
#include "a4988.h"

/* ---------- mock GPIO ---------- */

static void mock_set_output(void *ctx) { (void)ctx; }
static void mock_set_input(void *ctx)  { (void)ctx; }
static void mock_write_pin(void *ctx, bool level) { (void)ctx; (void)level; }
static bool mock_read_pin(void *ctx)   { (void)ctx; return false; }
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const a4988_gpio_t mock_gpio = {
    .set_output = mock_set_output,
    .set_input  = mock_set_input,
    .write_pin  = mock_write_pin,
    .read_pin   = mock_read_pin,
    .delay_us   = mock_delay_us,
    .ctx        = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    a4988_t dev;
    ASSERT_EQ(a4988_init(&dev, &mock_gpio), A4988_OK);
}

TEST(test_init_null)
{
    a4988_t dev;
    ASSERT_EQ(a4988_init(NULL, &mock_gpio), A4988_ERR_PARAM);
    ASSERT_EQ(a4988_init(&dev, NULL), A4988_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)A4988_ERR_PARAM);
    ASSERT_EQ(-2, (int)A4988_ERR_TIMEOUT);
}

/* TODO: add protocol-specific tests */

TEST_MAIN("a4988")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_error_codes);
TEST_MAIN_END()
