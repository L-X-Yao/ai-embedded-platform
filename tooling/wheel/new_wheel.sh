#!/bin/bash
# new_wheel.sh — scaffold a new wheel with all boilerplate.
#
# Usage:
#   new_wheel.sh --layer drivers --name bmp280 --hal i2c [--addr 0x76]
#   new_wheel.sh --layer drivers --name enc28j60 --hal spi
#   new_wheel.sh --layer drivers --name dht11 --hal gpio
#   new_wheel.sh --layer drivers --name ds18b20 --hal onewire
#   new_wheel.sh --layer drivers --name servo --hal pwm
#   new_wheel.sh --layer primitives --name ringbuf
#   new_wheel.sh --layer modules --name mqtt_client --deps mqtt_packet
#   new_wheel.sh --layer primitives --name kvs --vendor   # ADR-010 vendor mode
#   new_wheel.sh --layer drivers --name bmp280 --hal i2c --commit  # scaffold + git commit
#
# Generated files (standard):
#   wheels/<layer>/<name>/<name>.h       — header with HAL injection struct + error enum
#   wheels/<layer>/<name>/<name>.c       — implementation skeleton
#   wheels/<layer>/<name>/test/CMakeLists.txt
#   wheels/<layer>/<name>/test/test_<name>.c — mock HAL + test harness + 2 starter tests
#   wheels/<layer>/<name>/README.md      — candidate-level README
#
# Generated files (--vendor, ADR-010):
#   wheels/<layer>/<name>/vendor/inc/    — place upstream headers here
#   wheels/<layer>/<name>/vendor/src/    — place upstream sources here
#   wheels/<layer>/<name>/VENDOR.md      — upstream tracking template
#   wheels/<layer>/<name>/test/CMakeLists.txt
#   wheels/<layer>/<name>/test/test_<name>.c
#   wheels/<layer>/<name>/README.md      — with vendor-specific sections
#
# Also prints a CI job YAML snippet to paste into .github/workflows/ci.yml.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

# --- Args ---------------------------------------------------------------------

LAYER=""
NAME=""
HAL=""
ADDR=""
VENDOR=0
NO_AUDIT=0
COMMIT=0
DEPS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --layer)    LAYER="$2";  shift 2 ;;
        --name)     NAME="$2";   shift 2 ;;
        --hal)      HAL="$2";    shift 2 ;;
        --addr)     ADDR="$2";   shift 2 ;;
        --deps)     DEPS+=("$2"); shift 2 ;;
        --vendor)   VENDOR=1;    shift   ;;
        --no-audit) NO_AUDIT=1;  shift   ;;
        --commit)   COMMIT=1;    shift   ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

[[ -z "$LAYER" ]] && echo "Error: --layer required (primitives|drivers|protocols|modules)" && exit 1
[[ -z "$NAME" ]]  && echo "Error: --name required" && exit 1

if ! echo "$NAME" | grep -qE '^[a-z][a-z0-9_]*$'; then
    echo "Error: --name must match [a-z][a-z0-9_]* (got: $NAME)"
    exit 1
fi

if ! echo "$LAYER" | grep -qE '^(primitives|drivers|protocols|modules|_draft)$'; then
    echo "Error: --layer must be primitives|drivers|protocols|modules|_draft (got: $LAYER)"
    exit 1
fi

# Normalize
NAME_UPPER=$(echo "$NAME" | tr '[:lower:]' '[:upper:]')
NAME_LOWER=$(echo "$NAME" | tr '[:upper:]' '[:lower:]')

if [[ "$LAYER" == "drivers" && -z "$HAL" ]]; then
    echo "Error: --hal required for drivers (i2c|spi|gpio|onewire|pwm)"
    exit 1
fi

if [[ -n "$HAL" ]] && ! echo "$HAL" | grep -qE '^(i2c|spi|gpio|onewire|pwm)$'; then
    echo "Error: --hal must be i2c|spi|gpio|onewire|pwm (got: $HAL)"
    exit 1
fi

WHEEL_DIR="$REPO/wheels/$LAYER/$NAME_LOWER"
if [[ -d "$WHEEL_DIR" ]]; then
    echo "Error: $WHEEL_DIR already exists"
    exit 1
fi

# Map layer to level label (used in header/source file comments)
case "$LAYER" in
    primitives) LAYER_LABEL="L0 (primitives)" ;;
    drivers)    LAYER_LABEL="L1 (drivers)" ;;
    protocols)  LAYER_LABEL="L2 (protocols)" ;;
    modules)    LAYER_LABEL="L3 (modules)" ;;
    _draft)     LAYER_LABEL="draft" ;;
    *)          LAYER_LABEL="$LAYER" ;;
esac

# --- Resolve --deps paths -----------------------------------------------------
# Each dep is a wheel name (e.g. "mqtt_packet"). We find its directory and
# generate CMakeLists source + include entries.
DEPS_SOURCES=""    # extra .c files for CMakeLists add_executable
DEPS_INCLUDES=""   # extra include_directories entries

for dep in "${DEPS[@]}"; do
    dep_dir=$(find "$REPO/wheels" -maxdepth 3 -name "${dep}.h" 2>/dev/null | head -1 | xargs -r dirname)
    if [[ -z "$dep_dir" ]]; then
        echo "Error: --deps ${dep}: cannot find wheels/**/${dep}/${dep}.h"
        exit 1
    fi
    dep_rel="\${CMAKE_CURRENT_SOURCE_DIR}/../../../../${dep_dir#$REPO/}"
    DEPS_SOURCES="${DEPS_SOURCES}    ${dep_rel}/${dep}.c
"
    DEPS_INCLUDES="${DEPS_INCLUDES}    ${dep_rel}
"
done

# --- Generate -----------------------------------------------------------------

mkdir -p "$WHEEL_DIR/test"

# ---- Header ------------------------------------------------------------------

generate_header_i2c() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK        =  0,
    ${NAME_UPPER}_ERR_I2C   = -1,
    ${NAME_UPPER}_ERR_ID    = -2,
    ${NAME_UPPER}_ERR_PARAM = -3,
} ${NAME_LOWER}_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ${NAME_LOWER}_i2c_t;

/* ---------- constants ---------- */

#define ${NAME_UPPER}_DEV_ADDR  ${ADDR:-0x00u}

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} ${NAME_LOWER}_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ${NAME_LOWER}_i2c_t *i2c;
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev,
                               const ${NAME_LOWER}_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_read(${NAME_LOWER}_t *dev,
                               ${NAME_LOWER}_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_spi() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK          =  0,
    ${NAME_UPPER}_ERR_PARAM   = -1,
    ${NAME_UPPER}_ERR_SPI     = -2,
    ${NAME_UPPER}_ERR_ID      = -3,
    ${NAME_UPPER}_ERR_TIMEOUT = -4,
} ${NAME_LOWER}_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} ${NAME_LOWER}_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const ${NAME_LOWER}_spi_t *spi;
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev,
                               const ${NAME_LOWER}_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_gpio() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK          =  0,
    ${NAME_UPPER}_ERR_PARAM   = -1,
    ${NAME_UPPER}_ERR_TIMEOUT = -2,
} ${NAME_LOWER}_err_t;

/* ---------- GPIO HAL (caller provides) ---------- */

typedef struct {
    void (*set_output)(void *ctx);
    void (*set_input)(void *ctx);
    void (*write_pin)(void *ctx, bool level);
    bool (*read_pin)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} ${NAME_LOWER}_gpio_t;

/* ---------- device context ---------- */

typedef struct {
    const ${NAME_LOWER}_gpio_t *gpio;  /* caller-owned, must outlive dev */
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev, const ${NAME_LOWER}_gpio_t *gpio);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_onewire() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      onewire_hal (injected)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK            =  0,
    ${NAME_UPPER}_ERR_PARAM     = -1,
    ${NAME_UPPER}_ERR_NO_DEVICE = -2,
    ${NAME_UPPER}_ERR_CRC       = -3,
} ${NAME_LOWER}_err_t;

/* ---------- 1-Wire HAL (caller provides) ---------- */

typedef struct {
    bool    (*reset)(void *ctx);
    void    (*write_byte)(void *ctx, uint8_t byte);
    uint8_t (*read_byte)(void *ctx);
    void    *ctx;
} ${NAME_LOWER}_ow_t;

/* ---------- device context ---------- */

typedef struct {
    const ${NAME_LOWER}_ow_t *ow;  /* caller-owned, must outlive dev */
    uint8_t rom[8];
    bool    use_rom;
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev, const ${NAME_LOWER}_ow_t *ow);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_pwm() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK        =  0,
    ${NAME_UPPER}_ERR_PARAM = -1,
    ${NAME_UPPER}_ERR_HAL   = -2,
} ${NAME_LOWER}_err_t;

/* ---------- PWM HAL (caller provides) ---------- */

typedef struct {
    bool (*set_pulse_us)(void *ctx, uint32_t pulse_us);
    void *ctx;
} ${NAME_LOWER}_pwm_t;

/* ---------- device context ---------- */

typedef struct {
    const ${NAME_LOWER}_pwm_t *pwm;  /* caller-owned, must outlive dev */
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev, const ${NAME_LOWER}_pwm_t *pwm);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_none() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK        =  0,
    ${NAME_UPPER}_ERR_PARAM = -1,
} ${NAME_LOWER}_err_t;

/* ---------- instance ---------- */

typedef struct {
    int _placeholder; /* internal: replace with real state */
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Initialize instance. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *ctx);

/* TODO: add more API — see api-conventions.md for rules.
 * Add @thread_safe and @isr_safe annotations to every function declaration.
 * Mark internal struct fields with a comment. */

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

# ---- Implementation ----------------------------------------------------------

generate_c_i2c() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev,
                               const ${NAME_LOWER}_i2c_t *i2c)
{
    if (!dev || !i2c) return ${NAME_UPPER}_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return ${NAME_UPPER}_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, ${NAME_UPPER}_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return ${NAME_UPPER}_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return ${NAME_UPPER}_OK;
}

${NAME_LOWER}_err_t ${NAME_LOWER}_read(${NAME_LOWER}_t *dev,
                               ${NAME_LOWER}_data_t *out)
{
    if (!dev || !out) return ${NAME_UPPER}_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return ${NAME_UPPER}_ERR_I2C;
}
C_EOF
}

generate_c_spi() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"

/* TODO: command defines */

static uint8_t spi_cmd(const ${NAME_LOWER}_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev,
                               const ${NAME_LOWER}_spi_t *spi)
{
    if (!dev || !spi) return ${NAME_UPPER}_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return ${NAME_UPPER}_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return ${NAME_UPPER}_OK;
}
C_EOF
}

generate_c_gpio() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev, const ${NAME_LOWER}_gpio_t *gpio)
{
    if (!dev || !gpio)
        return ${NAME_UPPER}_ERR_PARAM;
    if (!gpio->set_output || !gpio->set_input || !gpio->write_pin ||
        !gpio->read_pin || !gpio->delay_us)
        return ${NAME_UPPER}_ERR_PARAM;
    dev->gpio = gpio;
    return ${NAME_UPPER}_OK;
}

/* TODO: implement read/write functions */
C_EOF
}

generate_c_onewire() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      onewire_hal (injected)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"
#include <string.h>

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev, const ${NAME_LOWER}_ow_t *ow)
{
    if (!dev || !ow)
        return ${NAME_UPPER}_ERR_PARAM;
    if (!ow->reset || !ow->write_byte || !ow->read_byte)
        return ${NAME_UPPER}_ERR_PARAM;
    dev->ow = ow;
    memset(dev->rom, 0, 8);
    dev->use_rom = false;
    return ${NAME_UPPER}_OK;
}

/* TODO: implement read_rom, start_conversion, read_data */
C_EOF
}

generate_c_pwm() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *dev, const ${NAME_LOWER}_pwm_t *pwm)
{
    if (!dev || !pwm)
        return ${NAME_UPPER}_ERR_PARAM;
    if (!pwm->set_pulse_us)
        return ${NAME_UPPER}_ERR_PARAM;
    dev->pwm = pwm;
    return ${NAME_UPPER}_OK;
}

/* TODO: implement control functions */
C_EOF
}

generate_c_none() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *ctx)
{
    if (!ctx) return ${NAME_UPPER}_ERR_PARAM;
    /* TODO: implement */
    return ${NAME_UPPER}_OK;
}
C_EOF
}

# ---- CMakeLists.txt ----------------------------------------------------------

cat > "$WHEEL_DIR/test/CMakeLists.txt" << CMAKE_EOF
cmake_minimum_required(VERSION 3.13)
project(test_${NAME_LOWER} C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_${NAME_LOWER}
    test_${NAME_LOWER}.c
    ../${NAME_LOWER}.c
${DEPS_SOURCES})

target_include_directories(test_${NAME_LOWER} PRIVATE
    \${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    \${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
${DEPS_INCLUDES})
target_compile_options(test_${NAME_LOWER} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ${NAME_LOWER}_tests COMMAND test_${NAME_LOWER})
CMAKE_EOF

# ---- Test file ---------------------------------------------------------------

generate_test_i2c() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include "mock_i2c.h"
#include "${NAME_LOWER}.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* TODO: set default register values (chip ID, etc.) */
    /* mock_regs[0xXX] = 0xYY; */
}

static const ${NAME_LOWER}_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static ${NAME_LOWER}_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &mock_i2c), ${NAME_UPPER}_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &mock_i2c), ${NAME_UPPER}_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(${NAME_LOWER}_init(NULL, &mock_i2c), ${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(${NAME_LOWER}_init(&dev, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    ${NAME_LOWER}_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ${NAME_LOWER}_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(${NAME_LOWER}_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_I2C);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_ID);
    ASSERT_EQ(-3, (int)${NAME_UPPER}_ERR_PARAM);
}

/* TODO: add more tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
TEST_MAIN_END()
TEST_EOF
}

generate_hw_model_i2c() {
    cat > "$WHEEL_DIR/test/hw_model_${NAME_LOWER}.h" << HW_EOF
/*
 * hw_model_${NAME_LOWER}.h — datasheet-accurate register model for ${NAME_LOWER}
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with power-on-reset defaults. Use in test_hw_model_${NAME_LOWER}.c
 * for round-trip verification against the real driver.
 */
#ifndef HW_MODEL_${NAME_UPPER}_H
#define HW_MODEL_${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    /* TODO: set POR defaults from datasheet */
    /* hw_regs[REG_WHO_AM_I] = 0xXX; */
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    /* TODO: enforce read-only registers, side effects */
    hw_regs[reg] = val;
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw_regs[(uint8_t)(reg + i)];
    }
    return true;
}

/* TODO: add inject helpers for physical values */
/* static void hw_model_inject_xxx(float value) { ... } */

#endif /* HW_MODEL_${NAME_UPPER}_H */
HW_EOF

    cat > "$WHEEL_DIR/test/test_hw_model_${NAME_LOWER}.c" << HWT_EOF
#include "test_harness.h"
#include "hw_model_${NAME_LOWER}.h"
#include "${NAME_LOWER}.h"

static const ${NAME_LOWER}_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static ${NAME_LOWER}_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &hw_i2c), ${NAME_UPPER}_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    ${NAME_LOWER}_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("${NAME_LOWER}_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
HWT_EOF

    # Append hw_model target to CMakeLists.txt
    cat >> "$WHEEL_DIR/test/CMakeLists.txt" << CMAKE_HW_EOF

# hw_model datasheet test
add_executable(test_hw_model_${NAME_LOWER}
    test_hw_model_${NAME_LOWER}.c
    ../${NAME_LOWER}.c
)
target_include_directories(test_hw_model_${NAME_LOWER} PRIVATE
    \${CMAKE_CURRENT_SOURCE_DIR}/..
    \${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_${NAME_LOWER} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME ${NAME_LOWER}_hw_model_tests COMMAND test_hw_model_${NAME_LOWER})
CMAKE_HW_EOF
}

generate_test_spi() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include <string.h>
#include "${NAME_LOWER}.h"

/* ---------- mock SPI ---------- */

static int     mock_fail_at;
static uint8_t mock_cs_state;

static uint8_t xfer_log[256];
static int     xfer_log_idx;

static void mock_reset(void)
{
    mock_fail_at   = -1;
    mock_cs_state  = 1;
    xfer_log_idx   = 0;
    memset(xfer_log, 0, sizeof(xfer_log));
}

static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (xfer_log_idx < (int)sizeof(xfer_log))
        xfer_log[xfer_log_idx++] = tx;
    return 0xFF; /* override for device-specific responses */
}

static const ${NAME_LOWER}_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ${NAME_LOWER}_t dev;
    ${NAME_LOWER}_err_t e = ${NAME_LOWER}_init(&dev, &mock_spi);
    ASSERT_EQ(e, ${NAME_UPPER}_OK);
}

TEST(test_null_safety)
{
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(NULL, &mock_spi), ${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(${NAME_LOWER}_init(&dev, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_init_spi_fail)
{
    /* TODO: implement once init reads chip ID via SPI.
     * Set mock to return wrong ID, verify ERR_ID returned. */
    (void)mock_fail_at;
    ASSERT_TRUE(${NAME_UPPER}_ERR_SPI < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_SPI);
    ASSERT_EQ(-3, (int)${NAME_UPPER}_ERR_ID);
    ASSERT_EQ(-4, (int)${NAME_UPPER}_ERR_TIMEOUT);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    ${NAME_LOWER}_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ${NAME_LOWER}_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(${NAME_LOWER}_init(&zero_dev, &zero_hal) < 0);
}

/* TODO: add more tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_init_ok);
    RUN(test_null_safety);
    RUN(test_init_spi_fail);
    RUN(test_error_codes);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
TEST_EOF
}

generate_hw_model_spi() {
    cat > "$WHEEL_DIR/test/hw_model_${NAME_LOWER}.h" << HW_EOF
/*
 * hw_model_${NAME_LOWER}.h — datasheet-accurate SPI register model for ${NAME_LOWER}
 *
 * Simulates SPI protocol: bit 7 of first byte = R/W flag (1=read).
 * Provides cs/xfer callbacks backed by a register array with POR defaults.
 */
#ifndef HW_MODEL_${NAME_UPPER}_H
#define HW_MODEL_${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_xfer_count;
static int     hw_cs_state;
static uint8_t hw_addr_latch;
static int     hw_byte_idx;

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_xfer_count = 0;
    hw_cs_state   = 1;
    hw_addr_latch = 0;
    hw_byte_idx   = 0;
    /* TODO: set POR defaults from datasheet */
    /* hw_regs[REG_WHO_AM_I] = 0xXX; */
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx) { (void)ctx; hw_cs_state = 0; hw_byte_idx = 0; }

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    hw_xfer_count++;
    if (hw_byte_idx == 0) {
        hw_addr_latch = tx & 0x7F;
        hw_byte_idx++;
        return 0xFF;
    }
    uint8_t reg = (uint8_t)(hw_addr_latch + hw_byte_idx - 1);
    uint8_t ret = hw_regs[reg];
    if (!(hw_addr_latch & 0x80)) {
        hw_regs[reg] = tx;
    }
    hw_byte_idx++;
    return ret;
}

/* TODO: add inject helpers for physical values */

#endif /* HW_MODEL_${NAME_UPPER}_H */
HW_EOF

    cat > "$WHEEL_DIR/test/test_hw_model_${NAME_LOWER}.c" << HWT_EOF
#include "test_harness.h"
#include "hw_model_${NAME_LOWER}.h"
#include "${NAME_LOWER}.h"

static const ${NAME_LOWER}_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static ${NAME_LOWER}_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID and succeeds */
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &hw_spi), ${NAME_UPPER}_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    ${NAME_LOWER}_init(&dev, &hw_spi);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("${NAME_LOWER}_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
HWT_EOF

    cat >> "$WHEEL_DIR/test/CMakeLists.txt" << CMAKE_HW_EOF

# hw_model datasheet test
add_executable(test_hw_model_${NAME_LOWER}
    test_hw_model_${NAME_LOWER}.c
    ../${NAME_LOWER}.c
)
target_include_directories(test_hw_model_${NAME_LOWER} PRIVATE
    \${CMAKE_CURRENT_SOURCE_DIR}/..
    \${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_${NAME_LOWER} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME ${NAME_LOWER}_hw_model_tests COMMAND test_hw_model_${NAME_LOWER})
CMAKE_HW_EOF
}

generate_test_gpio() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include "${NAME_LOWER}.h"

/* ---------- mock GPIO ---------- */

static void mock_set_output(void *ctx) { (void)ctx; }
static void mock_set_input(void *ctx)  { (void)ctx; }
static void mock_write_pin(void *ctx, bool level) { (void)ctx; (void)level; }
static bool mock_read_pin(void *ctx)   { (void)ctx; return false; }
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const ${NAME_LOWER}_gpio_t mock_gpio = {
    .set_output = mock_set_output,
    .set_input  = mock_set_input,
    .write_pin  = mock_write_pin,
    .read_pin   = mock_read_pin,
    .delay_us   = mock_delay_us,
    .ctx        = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &mock_gpio), ${NAME_UPPER}_OK);
}

TEST(test_init_null)
{
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(NULL, &mock_gpio), ${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(${NAME_LOWER}_init(&dev, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_TIMEOUT);
}

/* TODO: add protocol-specific tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_error_codes);
TEST_MAIN_END()
TEST_EOF
}

generate_test_onewire() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include <string.h>
#include "${NAME_LOWER}.h"

/* ---------- mock 1-Wire ---------- */

static bool mock_presence;

static bool mock_reset(void *ctx)      { (void)ctx; return mock_presence; }
static void mock_write_byte(void *ctx, uint8_t b) { (void)ctx; (void)b; }
static uint8_t mock_read_byte(void *ctx) { (void)ctx; return 0xFF; }

static const ${NAME_LOWER}_ow_t mock_ow = {
    .reset      = mock_reset,
    .write_byte = mock_write_byte,
    .read_byte  = mock_read_byte,
    .ctx        = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_presence = true;
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &mock_ow), ${NAME_UPPER}_OK);
}

TEST(test_init_null)
{
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(NULL, &mock_ow), ${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(${NAME_LOWER}_init(&dev, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_init_no_presence)
{
    /* TODO: implement once init does bus reset.
     * Set mock_presence=false, verify ERR_NO_DEVICE returned. */
    mock_presence = false;
    ASSERT_TRUE(${NAME_UPPER}_ERR_NO_DEVICE < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_NO_DEVICE);
    ASSERT_EQ(-3, (int)${NAME_UPPER}_ERR_CRC);
}

/* TODO: add device-specific tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_no_presence);
    RUN(test_error_codes);
TEST_MAIN_END()
TEST_EOF
}

generate_test_pwm() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include "${NAME_LOWER}.h"

/* ---------- mock PWM ---------- */

static uint32_t mock_last_pulse;
static bool mock_fail;

static bool mock_set_pulse(void *ctx, uint32_t pulse_us)
{
    (void)ctx;
    if (mock_fail) return false;
    mock_last_pulse = pulse_us;
    return true;
}

static const ${NAME_LOWER}_pwm_t mock_pwm = {
    .set_pulse_us = mock_set_pulse,
    .ctx = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_fail = false;
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(&dev, &mock_pwm), ${NAME_UPPER}_OK);
}

TEST(test_init_null)
{
    ${NAME_LOWER}_t dev;
    ASSERT_EQ(${NAME_LOWER}_init(NULL, &mock_pwm), ${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(${NAME_LOWER}_init(&dev, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_HAL);
}

/* TODO: add control tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_error_codes);
TEST_MAIN_END()
TEST_EOF
}

generate_header_protocol() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK           =  0,
    ${NAME_UPPER}_ERR_PARAM    = -1,
    ${NAME_UPPER}_ERR_FORMAT   = -2,
    ${NAME_UPPER}_ERR_SHORT    = -3,
    ${NAME_UPPER}_ERR_OVERFLOW = -4,
} ${NAME_LOWER}_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} ${NAME_LOWER}_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_parse(const uint8_t *data, size_t len,
                               ${NAME_LOWER}_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int ${NAME_LOWER}_build(uint8_t *buf, size_t cap,
                        const ${NAME_LOWER}_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_c_protocol() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"
#include <string.h>

${NAME_LOWER}_err_t ${NAME_LOWER}_parse(const uint8_t *data, size_t len,
                               ${NAME_LOWER}_msg_t *out)
{
    if (!data || !out) return ${NAME_UPPER}_ERR_PARAM;
    if (len < 2) return ${NAME_UPPER}_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return ${NAME_UPPER}_OK;
}

int ${NAME_LOWER}_build(uint8_t *buf, size_t cap,
                        const ${NAME_LOWER}_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
C_EOF
}

generate_test_protocol() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include "${NAME_LOWER}.h"
#include <string.h>

TEST(test_parse_ok)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    ${NAME_LOWER}_msg_t msg;
    ASSERT_EQ(${NAME_LOWER}_parse(data, sizeof(data), &msg), ${NAME_UPPER}_OK);
}

TEST(test_parse_null)
{
    ${NAME_LOWER}_msg_t msg;
    ASSERT_EQ(${NAME_LOWER}_parse(NULL, 0, &msg), ${NAME_UPPER}_ERR_PARAM);
    uint8_t data[] = {0x01};
    ASSERT_EQ(${NAME_LOWER}_parse(data, 1, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_parse_short)
{
    uint8_t data[] = {0x01};
    ${NAME_LOWER}_msg_t msg;
    ASSERT_EQ(${NAME_LOWER}_parse(data, 1, &msg), ${NAME_UPPER}_ERR_SHORT);
}

TEST(test_build_ok)
{
    ${NAME_LOWER}_msg_t msg = { .type = 0x01, .length = 2 };
    uint8_t buf[32];
    int n = ${NAME_LOWER}_build(buf, sizeof(buf), &msg);
    ASSERT_TRUE(n > 0);
}

TEST(test_build_null)
{
    ${NAME_LOWER}_msg_t msg = { .type = 0x01 };
    ASSERT_EQ(${NAME_LOWER}_build(NULL, 32, &msg), -1);
    uint8_t buf[32];
    ASSERT_EQ(${NAME_LOWER}_build(buf, 32, NULL), -1);
}

TEST(test_build_overflow)
{
    ${NAME_LOWER}_msg_t msg = { .type = 0x01 };
    uint8_t buf[1];
    ASSERT_EQ(${NAME_LOWER}_build(buf, 1, &msg), -1);
}

TEST(test_parse_adversarial)
{
    ${NAME_LOWER}_msg_t msg;
    /* All zeros */
    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    ${NAME_LOWER}_parse(zeros, sizeof(zeros), &msg);

    /* All 0xFF */
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof(ones));
    ${NAME_LOWER}_parse(ones, sizeof(ones), &msg);

    /* Boundary lengths: 0, 1, 255, 256 */
    uint8_t big[256];
    memset(big, 0xAA, sizeof(big));
    ${NAME_LOWER}_parse(big, 0, &msg);
    ${NAME_LOWER}_parse(big, 1, &msg);
    ${NAME_LOWER}_parse(big, 255, &msg);
    ${NAME_LOWER}_parse(big, 256, &msg);

    /* Must not crash on any input — return value irrelevant */
}

TEST(test_parse_mini_fuzz)
{
    ${NAME_LOWER}_msg_t msg;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[128];
        int len = (int)(seed % 129);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        ${NAME_LOWER}_parse(buf, (size_t)len, &msg);
    }
    /* 1000 random inputs — must not crash */
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)${NAME_UPPER}_OK);
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_FORMAT);
    ASSERT_EQ(-3, (int)${NAME_UPPER}_ERR_SHORT);
    ASSERT_EQ(-4, (int)${NAME_UPPER}_ERR_OVERFLOW);
}

TEST_MAIN("${NAME_LOWER}")
    RUN(test_parse_ok);
    RUN(test_parse_null);
    RUN(test_parse_short);
    RUN(test_build_ok);
    RUN(test_build_null);
    RUN(test_build_overflow);
    RUN(test_parse_adversarial);
    RUN(test_parse_mini_fuzz);
    RUN(test_error_codes);
TEST_MAIN_END()
TEST_EOF
}

generate_test_none() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include <string.h>
#include "${NAME_LOWER}.h"

TEST(test_zero_init_safety)
{
    ${NAME_LOWER}_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_EQ(${NAME_LOWER}_init(&ctx), ${NAME_UPPER}_OK);
}

/* TODO: add tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_zero_init_safety);
TEST_MAIN_END()
TEST_EOF
}

# ---- README ------------------------------------------------------------------

# (LAYER_LABEL is defined before generate_header calls, near top of script)

generate_standard_readme() {
    cat > "$WHEEL_DIR/README.md" << README_EOF
# ${NAME_LOWER} — TODO: one-line description

**Status: candidate**
**Layer: ${LAYER_LABEL}**

TODO: 2-3 sentence description.

## Origin

TODO: "Extracted from \`projects/<proj>/src/...\`" or "Proactive build for <use case>".

## Why self-written?

Searched GitHub/embedded ecosystem ($(date +%Y-%m-%d)):

- TODO: list evaluated alternatives and why they were not suitable.
- No suitable zero-dependency C99 implementation found.

Reference: TODO (or "None").

## Dependencies

TODO: list wheel dependencies (or "None.").

## API

\`\`\`c
/* TODO: document public API */
\`\`\`

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| TODO   | TODO    | TODO    |

## Supported platforms

- [x] Host (sim) — TODO: date
- [ ] TODO: real hardware target — not yet verified

## Known issues

TODO.

## How to test

\`\`\`bash
cd wheels/${LAYER}/${NAME_LOWER}/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_${NAME_LOWER}
\`\`\`

## Test status

- Unit tests (sim): TODO
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: TODO
- **Key decisions**: TODO

## Responsible human

@user
README_EOF
}

generate_standard_readme

# ---- Vendor mode (ADR-010) ---------------------------------------------------

generate_vendor_dirs() {
    mkdir -p "$WHEEL_DIR/vendor/inc"
    mkdir -p "$WHEEL_DIR/vendor/src"
}

generate_vendor_md() {
    cat > "$WHEEL_DIR/VENDOR.md" << VENDOR_EOF
# Upstream

- **Project**: TODO
- **Author**: TODO
- **URL**: TODO
- **Version**: TODO
- **License**: TODO
- **Vendored**: $(date +%Y-%m-%d)

## What was vendored

TODO: list files copied into vendor/inc/ and vendor/src/.

Excluded:
- TODO: list what was deliberately omitted and why.

## Source

TODO: describe where the files came from (GitHub release, user's copy, etc.).

## Update procedure

1. Download new release from upstream
2. Copy headers to \`vendor/inc/\`
3. Copy sources to \`vendor/src/\`
4. Run tests: \`cd test && cmake -GNinja -S . -B build && cmake --build build && ./build/test_${NAME_LOWER}\`
5. Update version and date in this file
VENDOR_EOF
}

generate_vendor_test_cmake() {
    cat > "$WHEEL_DIR/test/CMakeLists.txt" << CMAKE_EOF
cmake_minimum_required(VERSION 3.13)
project(test_${NAME_LOWER} C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_${NAME_LOWER}
    test_${NAME_LOWER}.c
    # TODO: add vendor sources
    # ../vendor/src/xxx.c
)

target_include_directories(test_${NAME_LOWER} PRIVATE
    \${CMAKE_CURRENT_SOURCE_DIR}/../vendor/inc
    \${CMAKE_CURRENT_SOURCE_DIR}
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    \${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
# Note: -Wall -Wextra -Wpedantic omitted for vendor wheels (upstream code triggers warnings)

enable_testing()
add_test(NAME ${NAME_LOWER}_tests COMMAND test_${NAME_LOWER})
CMAKE_EOF
}

generate_vendor_test() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include <string.h>

/* TODO: #include upstream header */

TEST(test_placeholder)
{
    /* TODO: add real tests for vendor API */
    ASSERT_TRUE(1);
}

/* TODO: add more tests */

TEST_MAIN("${NAME_LOWER}")
    RUN(test_placeholder);
TEST_MAIN_END()
TEST_EOF
}

generate_vendor_readme() {
    cat > "$WHEEL_DIR/README.md" << README_EOF
# ${NAME_LOWER} — TODO: one-line description

**Status: candidate**

TODO: 2-3 sentence description.

## Origin

Vendor: see VENDOR.md.

## Why this upstream?

TODO: why this library was chosen. List evaluated alternatives.
See ADR-010.

## Dependencies

TODO: list wheel dependencies (or "None.").

## API

Upstream API is used directly. Consumers \`#include <TODO.h>\`.

\`\`\`c
/* TODO: key API examples */
\`\`\`

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| TODO   | TODO    | TODO    |

## Supported platforms

- [x] Host (sim) — TODO: date
- [ ] TODO: real hardware target — not yet verified

## Known issues

None.

## How to test

\`\`\`bash
cd wheels/${LAYER}/${NAME_LOWER}/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_${NAME_LOWER}
\`\`\`

## Test status

- Unit tests (sim): TODO
- HIL: N/A

## API convention deviations

TODO: document any deviations from api-conventions.md.

## API review

- **Date**: TODO
- **Key decisions**: TODO

## Responsible human

@user
README_EOF
}

generate_header_module() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- configuration macros ---------- */

#ifndef ${NAME_UPPER}_MAX_ENTRIES
#define ${NAME_UPPER}_MAX_ENTRIES  16
#endif

/* ---------- error codes ---------- */

typedef enum {
    ${NAME_UPPER}_OK        =  0,
    ${NAME_UPPER}_ERR_PARAM = -1,
    ${NAME_UPPER}_ERR_FULL  = -2,
} ${NAME_LOWER}_err_t;

/* ---------- I/O abstraction (caller provides) ---------- */

typedef struct {
    void (*write)(void *ctx, const uint8_t *data, uint16_t len);
    void  *ctx;
} ${NAME_LOWER}_io_t;

/* ---------- module context ---------- */

typedef struct {
    const ${NAME_LOWER}_io_t *io;
    uint8_t entry_count;
} ${NAME_LOWER}_t;

/* ---------- API ---------- */

/** Initialize module. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *mod, const ${NAME_LOWER}_io_t *io);

/** Register an entry. Returns ERR_FULL if table is full. @thread_safe: no @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_register(${NAME_LOWER}_t *mod /* TODO: entry params */);

/** Process incoming data. @thread_safe: caller_must_lock @isr_safe: no */
${NAME_LOWER}_err_t ${NAME_LOWER}_process(${NAME_LOWER}_t *mod, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_c_module() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
// SPDX-License-Identifier: MIT
/*
 * ${NAME_LOWER} — TODO: one-line description
 *
 * Layer:     ${LAYER_LABEL%% (*}
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "${NAME_LOWER}.h"

${NAME_LOWER}_err_t ${NAME_LOWER}_init(${NAME_LOWER}_t *mod, const ${NAME_LOWER}_io_t *io)
{
    if (!mod || !io) return ${NAME_UPPER}_ERR_PARAM;
    mod->io = io;
    mod->entry_count = 0;
    return ${NAME_UPPER}_OK;
}

${NAME_LOWER}_err_t ${NAME_LOWER}_register(${NAME_LOWER}_t *mod /* TODO: entry params */)
{
    if (!mod) return ${NAME_UPPER}_ERR_PARAM;
    if (mod->entry_count >= ${NAME_UPPER}_MAX_ENTRIES)
        return ${NAME_UPPER}_ERR_FULL;
    /* TODO: store entry */
    mod->entry_count++;
    return ${NAME_UPPER}_OK;
}

${NAME_LOWER}_err_t ${NAME_LOWER}_process(${NAME_LOWER}_t *mod, const uint8_t *data, size_t len)
{
    if (!mod || !data) return ${NAME_UPPER}_ERR_PARAM;
    (void)len;
    /* TODO: implement processing */
    return ${NAME_UPPER}_OK;
}
C_EOF
}

generate_test_module() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include "test_harness.h"
#include "${NAME_LOWER}.h"
#include <string.h>

/* ---------- mock I/O ---------- */

static uint8_t write_buf[256];
static uint16_t write_len;

static void mock_write(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (len <= sizeof(write_buf)) {
        memcpy(write_buf, data, len);
        write_len = len;
    }
}

static const ${NAME_LOWER}_io_t mock_io = {
    .write = mock_write,
    .ctx   = NULL,
};

static void mock_reset(void)
{
    memset(write_buf, 0, sizeof(write_buf));
    write_len = 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ${NAME_LOWER}_t mod;
    ASSERT_EQ(${NAME_LOWER}_init(&mod, &mock_io), ${NAME_UPPER}_OK);
}

TEST(test_init_null)
{
    ${NAME_LOWER}_t mod;
    ASSERT_EQ(${NAME_LOWER}_init(NULL, &mock_io), ${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(${NAME_LOWER}_init(&mod, NULL), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_process_null)
{
    ${NAME_LOWER}_t mod;
    ${NAME_LOWER}_init(&mod, &mock_io);
    ASSERT_EQ(${NAME_LOWER}_process(&mod, NULL, 0), ${NAME_UPPER}_ERR_PARAM);
}

TEST(test_register_overflow)
{
    ${NAME_LOWER}_t mod;
    ${NAME_LOWER}_init(&mod, &mock_io);
    for (int i = 0; i < ${NAME_UPPER}_MAX_ENTRIES; i++) {
        ASSERT_EQ(${NAME_LOWER}_register(&mod), ${NAME_UPPER}_OK);
    }
    ASSERT_EQ(${NAME_LOWER}_register(&mod), ${NAME_UPPER}_ERR_FULL);
}

TEST(test_process_adversarial)
{
    ${NAME_LOWER}_t mod;
    ${NAME_LOWER}_init(&mod, &mock_io);

    /* All zeros */
    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    ${NAME_LOWER}_process(&mod, zeros, sizeof(zeros));

    /* All 0xFF */
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof(ones));
    ${NAME_LOWER}_process(&mod, ones, sizeof(ones));

    /* Boundary lengths */
    ${NAME_LOWER}_process(&mod, ones, 0);
    ${NAME_LOWER}_process(&mod, ones, 1);

    /* Must not crash on any input */
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)${NAME_UPPER}_OK);
    ASSERT_EQ(-1, (int)${NAME_UPPER}_ERR_PARAM);
    ASSERT_EQ(-2, (int)${NAME_UPPER}_ERR_FULL);
}

TEST_MAIN("${NAME_LOWER}")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_process_null);
    RUN(test_register_overflow);
    RUN(test_process_adversarial);
    RUN(test_error_codes);
TEST_MAIN_END()
TEST_EOF
}

# ---- Dispatch generation -----------------------------------------------------

if [[ "$VENDOR" -eq 1 ]]; then
    generate_vendor_dirs
    generate_vendor_md
    generate_vendor_test_cmake
    generate_vendor_test
    generate_vendor_readme
else
    # Auto-select template for protocols and modules layers
    EFFECTIVE_HAL="${HAL:-none}"
    if [[ "$LAYER" == "protocols" && "$EFFECTIVE_HAL" == "none" ]]; then
        EFFECTIVE_HAL="protocol"
    fi
    if [[ "$LAYER" == "modules" && "$EFFECTIVE_HAL" == "none" ]]; then
        EFFECTIVE_HAL="module"
    fi

    case "$EFFECTIVE_HAL" in
        i2c)      generate_header_i2c;      generate_c_i2c;      generate_test_i2c;      generate_hw_model_i2c ;;
        spi)      generate_header_spi;      generate_c_spi;      generate_test_spi;      generate_hw_model_spi ;;
        gpio)     generate_header_gpio;     generate_c_gpio;     generate_test_gpio     ;;
        onewire)  generate_header_onewire;  generate_c_onewire;  generate_test_onewire  ;;
        pwm)      generate_header_pwm;      generate_c_pwm;      generate_test_pwm      ;;
        protocol) generate_header_protocol; generate_c_protocol; generate_test_protocol ;;
        module)   generate_header_module;   generate_c_module;   generate_test_module   ;;
        *)        generate_header_none;     generate_c_none;     generate_test_none     ;;
    esac
fi

# ---- CI snippet --------------------------------------------------------------

echo ""
echo "=== Created: $WHEEL_DIR ==="
echo ""
find "$WHEEL_DIR" -type f | sort | while read -r f; do
    echo "  ${f#$REPO/}"
done

echo ""

# ---- CI coverage note --------------------------------------------------------

echo "CI: auto-discovered via wheels/*/*/test/CMakeLists.txt (no manual ci.yml edit needed)"
if [[ "$NAME_LOWER" == "log" || "$NAME_LOWER" == "crc16" ]]; then
    echo "CI: NOTE — log and crc16 have standalone jobs with pytest. Add a similar job if this wheel needs pytest."
fi

# ---- Auto-run audit ----------------------------------------------------------

if [[ "$NO_AUDIT" -eq 0 ]]; then
    echo ""
    echo "=== Running audit for ${NAME_LOWER} ==="
    if python3 "$REPO/tooling/audit/audit_wheels.py" --quiet 2>&1; then
        echo "Audit: passed"
    else
        echo "Audit: warnings found (review output above)"
    fi
fi

echo ""
echo "Done. Next steps:"
if [[ "$VENDOR" -eq 1 ]]; then
echo "  1. Copy upstream headers to vendor/inc/ and sources to vendor/src/"
echo "  2. Fill in VENDOR.md (upstream URL, version, license, file list)"
echo "  3. Fill in TODO placeholders in README.md"
echo "  4. Wire vendor sources in test/CMakeLists.txt"
echo "  5. Add tests to test/test_${NAME_LOWER}.c"
echo "  6. Run: python3 tooling/codegen/gen_null_tests.py wheels/${LAYER}/${NAME_LOWER}/${NAME_LOWER}.h"
echo "  7. Run: tooling/test/test_all.sh ${NAME_LOWER}"
else
echo "  1. Fill in TODO placeholders in ${NAME_LOWER}.h / ${NAME_LOWER}.c"
echo "  2. Add tests to test/test_${NAME_LOWER}.c"
echo "  3. Run: python3 tooling/codegen/gen_null_tests.py wheels/${LAYER}/${NAME_LOWER}/${NAME_LOWER}.h"
if [[ "$LAYER" == "drivers" && ("$HAL" == "i2c" || "$HAL" == "spi") ]]; then
echo "  4. Fill TODOs in test/hw_model_${NAME_LOWER}.h (POR defaults, inject helpers)"
echo "  5. Run: tooling/test/test_all.sh ${NAME_LOWER}"
else
echo "  4. Run: tooling/test/test_all.sh ${NAME_LOWER}"
fi
fi

# --- Optional: commit scaffold ---
if [[ $COMMIT -eq 1 ]]; then
    cd "$REPO"
    git add "wheels/${LAYER}/${NAME_LOWER}/"
    git commit -m "wheel(${LAYER}/${NAME_LOWER}): scaffold ${NAME_LOWER}"
    echo ""
    echo "Scaffold committed to git."
fi
