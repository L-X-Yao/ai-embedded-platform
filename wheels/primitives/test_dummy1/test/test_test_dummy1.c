#include "test_harness.h"
#include <string.h>
#include "test_dummy1.h"

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
}

/* TODO: add tests */

TEST_MAIN("test_dummy1")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
