#include "test_harness.h"
#include <string.h>
#include "memory_pool.h"

TEST(test_zero_init_safety)
{
    memory_pool_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_EQ(memory_pool_init(&ctx), MEMORY_POOL_OK);
}

/* TODO: add tests */

TEST_MAIN("memory_pool")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
