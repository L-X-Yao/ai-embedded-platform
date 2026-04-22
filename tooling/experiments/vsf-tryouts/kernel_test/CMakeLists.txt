cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{VSF_PATH})
    get_filename_component(__VSF_PATH ../../../../vsf ABSOLUTE)
    set(ENV{VSF_PATH} ${__VSF_PATH})
endif()
message(STATUS "VSF_PATH: $ENV{VSF_PATH}")

project(vsf_kernel_test_sim)

set(VSF_TARGET generic/linux64)

# Minimal VSF_USE: only linux simple libc. No LVGL/SDL2/FreeType/mbedtls.
set(VSF_USE_LINUX 1)
set(VSF_LINUX_USE_SIMPLE_LIBC 1)

# Point VSF cmake at our own config dir (contains vsf_usr_cfg.h)
set(VSF_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/config)

include($ENV{VSF_PATH}/script/cmake/vsf.cmake)

target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)
