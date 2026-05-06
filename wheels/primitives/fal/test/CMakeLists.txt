cmake_minimum_required(VERSION 3.13)
project(test_fal C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

set(STRICT_FLAGS -Wall -Wextra -Wpedantic -Werror -Wconversion)

# ---------- unit tests ----------

add_executable(test_fal
    test_fal.c
    ../fal.c
)
target_include_directories(test_fal PRIVATE ${COMMON_INCLUDE})
target_compile_options(test_fal PRIVATE ${STRICT_FLAGS})

# ---------- NULL parameter tests (auto-generated) ----------
# Uses mock_fal_dev.c for the mock flash device (test_fal.c has its own
# self-contained mock; test_null_fal.c is standalone and needs a separate one).

add_executable(test_null_fal
    test_null_fal.c
    mock_fal_dev.c
    ../fal.c
)
target_include_directories(test_null_fal PRIVATE ${COMMON_INCLUDE})
target_compile_options(test_null_fal PRIVATE ${STRICT_FLAGS})

# ---------- property tests ----------

add_executable(prop_fal
    prop_fal.c
    ../fal.c
)
target_include_directories(prop_fal PRIVATE ${COMMON_INCLUDE})
target_compile_options(prop_fal PRIVATE ${STRICT_FLAGS})

enable_testing()
add_test(NAME fal_tests      COMMAND test_fal)
add_test(NAME fal_null_tests COMMAND test_null_fal)
add_test(NAME fal_prop_tests COMMAND prop_fal)
