#include "test_harness.h"
#include <string.h>
#include "font_5x7.h"

TEST(test_zero_init_safety)
{
    font_5x7_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_EQ(font_5x7_init(&ctx), FONT_5X7_OK);
}

/* TODO: add tests */

TEST_MAIN("font_5x7")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
