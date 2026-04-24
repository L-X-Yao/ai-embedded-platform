vsf_add_sources(
    driver.c
)

vsf_add_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/Include
)

add_subdirectory_existed(gpio)
add_subdirectory_existed(usart)
add_subdirectory_existed(flash)
add_subdirectory_existed(spi)
add_subdirectory_existed(i2c)
