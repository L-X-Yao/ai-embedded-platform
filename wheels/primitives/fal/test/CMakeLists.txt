cmake_minimum_required(VERSION 3.13)
project(test_fal C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
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
    mock_fal_dev.c
    ../fal.c
)
target_include_directories(prop_fal PRIVATE ${COMMON_INCLUDE})
target_compile_options(prop_fal PRIVATE ${STRICT_FLAGS})


add_executable(test_boundary_fal
    test_boundary_fal.c
    mock_fal_dev.c
    ../fal.c
)
target_include_directories(test_boundary_fal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_fal PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME fal_tests      COMMAND test_fal)
add_test(NAME fal_null_tests COMMAND test_null_fal)
add_test(NAME fal_prop_tests COMMAND prop_fal)
add_test(NAME fal_boundary_tests COMMAND test_boundary_fal)

# ---------- partition tables ----------
# The table is compile-time, so each one needs its own build.

# The tooling reads target names and compile options straight from this file,
# so they are spelled out per target rather than built in a loop.
function(configure_table_test target table)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/bad_table
        ${COMMON_INCLUDE}
    )
    target_compile_definitions(${target} PRIVATE TABLE_${table})
    add_test(NAME fal_${target}_tests COMMAND ${target})
endfunction()

add_executable(test_table_past_end test_table_fal.c ../fal.c)
configure_table_test(test_table_past_end PAST_END)
target_compile_options(test_table_past_end PRIVATE ${STRICT_FLAGS})
add_executable(test_table_offset_past_end test_table_fal.c ../fal.c)
configure_table_test(test_table_offset_past_end OFFSET_PAST_END)
target_compile_options(test_table_offset_past_end PRIVATE ${STRICT_FLAGS})
add_executable(test_table_negative_offset test_table_fal.c ../fal.c)
configure_table_test(test_table_negative_offset NEGATIVE_OFFSET)
target_compile_options(test_table_negative_offset PRIVATE ${STRICT_FLAGS})
add_executable(test_table_wrapping_len test_table_fal.c ../fal.c)
configure_table_test(test_table_wrapping_len WRAPPING_LEN)
target_compile_options(test_table_wrapping_len PRIVATE ${STRICT_FLAGS})
add_executable(test_table_unknown_device test_table_fal.c ../fal.c)
configure_table_test(test_table_unknown_device UNKNOWN_DEVICE)
target_compile_options(test_table_unknown_device PRIVATE ${STRICT_FLAGS})
