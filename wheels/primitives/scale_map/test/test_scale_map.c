#include "test_harness.h"
#include "scale_map.h"

/* ---------- i32 ---------- */

TEST(test_i32_midpoint)
{
    ASSERT_EQ(scale_map_i32(512, 0, 1023, 0, 255), 127);
}

TEST(test_i32_identity)
{
    ASSERT_EQ(scale_map_i32(50, 0, 100, 0, 100), 50);
}

TEST(test_i32_invert)
{
    ASSERT_EQ(scale_map_i32(0, 0, 100, 100, 0), 100);
    ASSERT_EQ(scale_map_i32(100, 0, 100, 100, 0), 0);
}

TEST(test_i32_negative_range)
{
    ASSERT_EQ(scale_map_i32(0, -100, 100, 0, 200), 100);
}

TEST(test_i32_zero_input_range)
{
    ASSERT_EQ(scale_map_i32(5, 5, 5, 0, 100), 0);
}

TEST(test_i32_adc_to_mv)
{
    ASSERT_EQ(scale_map_i32(2048, 0, 4095, 0, 3300), 1650);
}

/* ---------- i32 clamp ---------- */

TEST(test_i32_clamp_in_range)
{
    ASSERT_EQ(scale_map_i32_clamp(50, 0, 100, 0, 255), 127);
}

TEST(test_i32_clamp_below)
{
    ASSERT_EQ(scale_map_i32_clamp(-10, 0, 100, 0, 255), 0);
}

TEST(test_i32_clamp_above)
{
    ASSERT_EQ(scale_map_i32_clamp(110, 0, 100, 0, 255), 255);
}

TEST(test_i32_clamp_inverted_output)
{
    ASSERT_EQ(scale_map_i32_clamp(-10, 0, 100, 255, 0), 255);
    ASSERT_EQ(scale_map_i32_clamp(110, 0, 100, 255, 0), 0);
}

/* ---------- float ---------- */

TEST(test_f_midpoint)
{
    ASSERT_NEAR(scale_map_f(0.5f, 0.0f, 1.0f, 0.0f, 100.0f), 50.0f, 0.01f);
}

TEST(test_f_negative)
{
    ASSERT_NEAR(scale_map_f(0.0f, -1.0f, 1.0f, -100.0f, 100.0f), 0.0f, 0.01f);
}

TEST(test_f_zero_range)
{
    ASSERT_NEAR(scale_map_f(5.0f, 5.0f, 5.0f, 0.0f, 100.0f), 0.0f, 0.01f);
}

TEST(test_f_celsius_to_fahrenheit)
{
    ASSERT_NEAR(scale_map_f(100.0f, 0.0f, 100.0f, 32.0f, 212.0f), 212.0f, 0.01f);
    ASSERT_NEAR(scale_map_f(0.0f, 0.0f, 100.0f, 32.0f, 212.0f), 32.0f, 0.01f);
}

/* ---------- float clamp ---------- */

TEST(test_f_clamp_in_range)
{
    ASSERT_NEAR(scale_map_f_clamp(0.5f, 0.0f, 1.0f, 0.0f, 100.0f), 50.0f, 0.01f);
}

TEST(test_f_clamp_below)
{
    ASSERT_NEAR(scale_map_f_clamp(-0.5f, 0.0f, 1.0f, 0.0f, 100.0f), 0.0f, 0.01f);
}

TEST(test_f_clamp_above)
{
    ASSERT_NEAR(scale_map_f_clamp(1.5f, 0.0f, 1.0f, 0.0f, 100.0f), 100.0f, 0.01f);
}

TEST_MAIN("scale_map")
    RUN(test_i32_midpoint);
    RUN(test_i32_identity);
    RUN(test_i32_invert);
    RUN(test_i32_negative_range);
    RUN(test_i32_zero_input_range);
    RUN(test_i32_adc_to_mv);
    RUN(test_i32_clamp_in_range);
    RUN(test_i32_clamp_below);
    RUN(test_i32_clamp_above);
    RUN(test_i32_clamp_inverted_output);
    RUN(test_f_midpoint);
    RUN(test_f_negative);
    RUN(test_f_zero_range);
    RUN(test_f_celsius_to_fahrenheit);
    RUN(test_f_clamp_in_range);
    RUN(test_f_clamp_below);
    RUN(test_f_clamp_above);
TEST_MAIN_END()
