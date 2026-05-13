#include "test_harness.h"
#include <string.h>
#include "peak_detector.h"

TEST(test_zero_init_safety)
{
    peak_detector_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_EQ(peak_detector_init(&ctx), PEAK_DETECTOR_OK);
}

/* TODO: add tests */

TEST_MAIN("peak_detector")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
