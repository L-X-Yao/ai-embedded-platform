cmake_minimum_required(VERSION 3.13)
project(crypto_pipeline C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(crypto_pipeline
    main.c
    ${WHEELS}/primitives/sha256/sha256.c
    ${WHEELS}/primitives/hmac_sha256/hmac_sha256.c
    ${WHEELS}/primitives/aes128/aes128.c
    ${WHEELS}/primitives/base64/base64.c
    ${WHEELS}/primitives/hex_codec/hex_codec.c
)

target_include_directories(crypto_pipeline PRIVATE
    ${WHEELS}/primitives/sha256
    ${WHEELS}/primitives/hmac_sha256
    ${WHEELS}/primitives/aes128
    ${WHEELS}/primitives/base64
    ${WHEELS}/primitives/hex_codec
    ${TOOLING}/harness
)

target_compile_options(crypto_pipeline PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
