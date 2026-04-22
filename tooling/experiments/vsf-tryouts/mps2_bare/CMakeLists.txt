cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{VSF_PATH})
    get_filename_component(__VSF_PATH ../../../../vsf ABSOLUTE)
    set(ENV{VSF_PATH} ${__VSF_PATH})
endif()
message(STATUS "VSF_PATH: $ENV{VSF_PATH}")

# arm-none-eabi-gcc fails cmake's compiler check because it can't link a
# host-targeted test binary. Set flags before project() so the check uses them.
set(CMAKE_C_FLAGS
    "-Ofast"
    CACHE INTERNAL "C compiler common flags"
)
set(CMAKE_CXX_FLAGS
    "-Ofast"
    CACHE INTERNAL "C++ compiler common flags"
)
include($ENV{VSF_PATH}/script/cmake/compilers/gnuarmemb.cmake)

project(vsf_mps2_bare)

set(VSF_TARGET an500)

# Minimal kernel-only config — no Linux userspace layer, no GUI stack.
set(VSF_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/config)
include($ENV{VSF_PATH}/script/cmake/vsf.cmake)

# Wrapper symbol rewrite (same pattern as template/cmake/mps2/CMakeLists.txt).
foreach(api ${VSF_TARGET_WRAPPER_LIST})
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        "${api}=__wrap_${api}"
    )
endforeach()

# Linker script — reuse the one VSF ships with the mps2 template (really an
# aic8800 layout, but FLASH@0x0/RAM@0x20000000 matches mps2-an500's memory map).
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    LINKER:-T$ENV{VSF_PATH}/example/template/project/cmake/mps2/linker.ld
    LINKER:-Map=output.map
    # cortex_m_generic.c uses _stext/_etext for trace text region probing but
    # the aic8800-derived linker.ld only defines __etext. Alias them.
    LINKER:--defsym=_stext=0
    LINKER:--defsym=_etext=__etext
)

# Integration demo: consume wheels/_draft/log from the MCU-sim side to prove
# the wheel survives outside host pytest (different target, different sink,
# different time source). See main.c for the semihosting sink.
get_filename_component(LOG_WHEEL_DIR ../../../../wheels/_draft/log ABSOLUTE)

target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${LOG_WHEEL_DIR}/log.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${LOG_WHEEL_DIR}
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    SUFFIX ".elf"
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
