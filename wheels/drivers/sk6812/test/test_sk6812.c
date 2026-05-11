#include "test_harness.h"
#include "sk6812.h"

/* ---------- mock GPIO ---------- */

static void mock_set_output(void *ctx) { (void)ctx; }
static void mock_set_input(void *ctx)  { (void)ctx; }
static void mock_write_pin(void *ctx, bool level) { (void)ctx; (void)level; }
static bool mock_read_pin(void *ctx)   { (void)ctx; return false; }
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const sk6812_gpio_t mock_gpio = {
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
    sk6812_t dev;
    ASSERT_EQ(sk6812_init(&dev, &mock_gpio), SK6812_OK);
}

TEST(test_init_null)
{
    sk6812_t dev;
    ASSERT_EQ(sk6812_init(NULL, &mock_gpio), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_init(&dev, NULL), SK6812_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)SK6812_ERR_PARAM);
    ASSERT_EQ(-2, (int)SK6812_ERR_TIMEOUT);
}

/* TODO: add protocol-specific tests */

TEST_MAIN("sk6812")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_error_codes);
TEST_MAIN_END()
