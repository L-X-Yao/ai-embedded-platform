#include "test_harness.h"
#include "scale_map.h"

/* All functions take value parameters only — no NULL to test. */

TEST(test_value_only_api)
{
    /* Placeholder: scale_map has no pointer parameters. */
    ASSERT_EQ(0, 0);
}

TEST_MAIN("scale_map_null")
    RUN(test_value_only_api);
TEST_MAIN_END()
