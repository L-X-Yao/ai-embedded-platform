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

foreach(table PAST_END OFFSET_PAST_END NEGATIVE_OFFSET WRAPPING_LEN UNKNOWN_DEVICE)
    string(TOLOWER ${table} name)
    add_executable(test_table_${name}
        test_table_fal.c
        ../fal.c
    )
    target_include_directories(test_table_${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/bad_table
        ${COMMON_INCLUDE}
    )
    target_compile_definitions(test_table_${name} PRIVATE TABLE_${table})
    target_compile_options(test_table_${name} PRIVATE ${STRICT_FLAGS})
    add_test(NAME fal_table_${name}_tests COMMAND test_table_${name})
endforeach()
