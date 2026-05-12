cmake_minimum_required(VERSION 3.13)
project(motion_control_adv C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(DRV       ${WHEELS}/drivers)

# ----- executable: motion_control_adv -----
add_executable(motion_control_adv
    motion_control_adv.c

    # L1 - mpu6050
    ${DRV}/mpu6050/mpu6050.c

    # L1 - servo
    ${DRV}/servo/servo.c

    # L1 - stepper
    ${DRV}/stepper/stepper.c

    # L1 - pca9685
    ${DRV}/pca9685/pca9685.c

    # L1 - drv8833
    ${DRV}/drv8833/drv8833.c

    # L1 - drv2605
    ${DRV}/drv2605/drv2605.c
)

target_include_directories(motion_control_adv PRIVATE
    # wheel headers
    ${DRV}/mpu6050
    ${DRV}/servo
    ${DRV}/stepper
    ${DRV}/pca9685
    ${DRV}/drv8833
    ${DRV}/drv2605

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(motion_control_adv PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

# Link math library for atan2
target_link_libraries(motion_control_adv PRIVATE m)

enable_testing()
add_test(NAME motion_control_adv_integration COMMAND motion_control_adv)
