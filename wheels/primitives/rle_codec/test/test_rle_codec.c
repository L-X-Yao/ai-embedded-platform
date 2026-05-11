#include "test_harness.h"
#include <string.h>
#include "rle_codec.h"

TEST(test_zero_init_safety)
{
    rle_codec_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_EQ(rle_codec_init(&ctx), RLE_CODEC_OK);
}

/* TODO: add tests */

TEST_MAIN("rle_codec")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
