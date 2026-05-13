#include "test_harness.h"
#include "scale_map.h"

/* scale_map functions take only value parameters (no pointers).
 * This file exists to satisfy the audit null-test requirement. */

TEST(test_no_pointer_params)
{
    /* All functions are pure value-in value-out — no NULL to test */
    ASSERT_EQ(scale_map_i32(0, 0, 100, 0, 100), 0);
}

TEST_MAIN("scale_map_null")
    RUN(test_no_pointer_params);
TEST_MAIN_END()
