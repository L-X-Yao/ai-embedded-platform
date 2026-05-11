#include "test_harness.h"
#include <string.h>
#include "double_buffer.h"

TEST(test_zero_init_safety)
{
    double_buffer_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_EQ(double_buffer_init(&ctx), DOUBLE_BUFFER_OK);
}

/* TODO: add tests */

TEST_MAIN("double_buffer")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
