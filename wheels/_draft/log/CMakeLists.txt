cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{VSF_PATH})
    get_filename_component(__VSF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../vsf" ABSOLUTE)
    set(ENV{VSF_PATH} "${__VSF_PATH}")
endif()

project(log_test C)

set(VSF_TARGET generic/linux64)
set(VSF_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include($ENV{VSF_PATH}/script/cmake/vsf.cmake)

target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
