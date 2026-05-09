cmake_minimum_required(VERSION 3.13)
project(test_flash_journal C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_flash_journal
    test_flash_journal.c
    ../flash_journal.c
)

target_include_directories(test_flash_journal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_flash_journal PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_flash_journal
    test_null_flash_journal.c
    ../flash_journal.c
)
target_include_directories(test_null_flash_journal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_flash_journal PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME flash_journal_tests COMMAND test_flash_journal)
add_test(NAME flash_journal_null_tests COMMAND test_null_flash_journal)
