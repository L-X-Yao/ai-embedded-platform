cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{VSF_PATH})
    get_filename_component(__VSF_PATH ../../../../vsf ABSOLUTE)
    set(ENV{VSF_PATH} ${__VSF_PATH})
endif()
message(STATUS "VSF_PATH: $ENV{VSF_PATH}")

set(CMAKE_C_FLAGS
    "-Os"
    CACHE INTERNAL "C compiler common flags"
)
set(CMAKE_CXX_FLAGS
    "-Os"
    CACHE INTERNAL "C++ compiler common flags"
)
include($ENV{VSF_PATH}/script/cmake/compilers/gnuarmemb.cmake)

project(vsf_linux_shell_size)

set(VSF_TARGET an500)
set(VSF_USE_LINUX 1)
set(VSF_LINUX_USE_SIMPLE_LIBC 1)

set(VSF_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/config)
include($ENV{VSF_PATH}/script/cmake/vsf.cmake)

foreach(api ${VSF_TARGET_WRAPPER_LIST})
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        "${api}=__wrap_${api}"
    )
endforeach()

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    LINKER:-T$ENV{VSF_PATH}/example/template/project/cmake/mps2/linker.ld
    LINKER:-Map=output.map
    LINKER:--defsym=_stext=0
    LINKER:--defsym=_etext=__etext
)

target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    SUFFIX ".elf"
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
