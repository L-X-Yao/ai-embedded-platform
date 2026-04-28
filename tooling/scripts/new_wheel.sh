#!/bin/bash
# new_wheel.sh — scaffold a new wheel with all boilerplate.
#
# Usage:
#   new_wheel.sh --layer drivers --name bmp280 --hal i2c [--addr 0x76]
#   new_wheel.sh --layer drivers --name enc28j60 --hal spi
#   new_wheel.sh --layer primitives --name ringbuf
#   new_wheel.sh --layer primitives --name kvs --vendor   # ADR-010 vendor mode
#
# Generated files (standard):
#   wheels/<layer>/<name>/<name>.h       — header with HAL injection struct + error enum
#   wheels/<layer>/<name>/<name>.c       — implementation skeleton
#   wheels/<layer>/<name>/test/CMakeLists.txt
#   wheels/<layer>/<name>/test/test_<name>.c — mock HAL + test harness + 2 starter tests
#   wheels/<layer>/<name>/README.md      — with "Why not VSF native?" section
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

while [[ $# -gt 0 ]]; do
    case "$1" in
        --layer)  LAYER="$2";  shift 2 ;;
        --name)   NAME="$2";   shift 2 ;;
        --hal)    HAL="$2";    shift 2 ;;
        --addr)   ADDR="$2";   shift 2 ;;
        --vendor) VENDOR=1;    shift   ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

[[ -z "$LAYER" ]] && echo "Error: --layer required (primitives|drivers|protocols|modules)" && exit 1
[[ -z "$NAME" ]]  && echo "Error: --name required" && exit 1

# Normalize
NAME_UPPER=$(echo "$NAME" | tr '[:lower:]' '[:upper:]')
NAME_LOWER=$(echo "$NAME" | tr '[:upper:]' '[:lower:]')

if [[ "$LAYER" == "drivers" && -z "$HAL" ]]; then
    echo "Error: --hal required for drivers (i2c|spi)"
    exit 1
fi

WHEEL_DIR="$REPO/wheels/$LAYER/$NAME_LOWER"
if [[ -d "$WHEEL_DIR" ]]; then
    echo "Error: $WHEEL_DIR already exists"
    exit 1
fi

# --- Generate -----------------------------------------------------------------

mkdir -p "$WHEEL_DIR/test"

# ---- Header ------------------------------------------------------------------

generate_header_i2c() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ${NAME_UPPER}_OK = 0,
    ${NAME_UPPER}_ERR_I2C,
    ${NAME_UPPER}_ERR_ID,
    ${NAME_UPPER}_ERR_NOT_READY,
} ${NAME_LOWER}_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} ${NAME_LOWER}_i2c_t;

#define ${NAME_UPPER}_DEV_ADDR  ${ADDR:-0x00u}

typedef struct {
    float x, y, z;
} ${NAME_LOWER}_data_t;

${NAME_LOWER}_err_t ${NAME_LOWER}_init(const ${NAME_LOWER}_i2c_t *i2c);

${NAME_LOWER}_err_t ${NAME_LOWER}_read(const ${NAME_LOWER}_i2c_t *i2c, ${NAME_LOWER}_data_t *out);

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_spi() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ${NAME_UPPER}_OK = 0,
    ${NAME_UPPER}_ERR_SPI,
    ${NAME_UPPER}_ERR_ID,
    ${NAME_UPPER}_ERR_TIMEOUT,
} ${NAME_LOWER}_err_t;

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} ${NAME_LOWER}_spi_t;

${NAME_LOWER}_err_t ${NAME_LOWER}_init(const ${NAME_LOWER}_spi_t *spi);

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

generate_header_none() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.h" << HEADER_EOF
#ifndef ${NAME_UPPER}_H
#define ${NAME_UPPER}_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ${NAME_UPPER}_OK = 0,
    ${NAME_UPPER}_ERR_PARAM,
} ${NAME_LOWER}_err_t;

/* TODO: define API */

#endif /* ${NAME_UPPER}_H */
HEADER_EOF
}

# ---- Implementation ----------------------------------------------------------

generate_c_i2c() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
#include "${NAME_LOWER}.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

${NAME_LOWER}_err_t ${NAME_LOWER}_init(const ${NAME_LOWER}_i2c_t *i2c)
{
    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, ${NAME_UPPER}_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return ${NAME_UPPER}_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return ${NAME_UPPER}_OK;
}

${NAME_LOWER}_err_t ${NAME_LOWER}_read(const ${NAME_LOWER}_i2c_t *i2c, ${NAME_LOWER}_data_t *out)
{
    (void)i2c; (void)out;
    /* TODO: implement */
    return ${NAME_UPPER}_ERR_NOT_READY;
}
C_EOF
}

generate_c_spi() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
#include "${NAME_LOWER}.h"

/* TODO: command defines */

static uint8_t spi_cmd(const ${NAME_LOWER}_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

${NAME_LOWER}_err_t ${NAME_LOWER}_init(const ${NAME_LOWER}_spi_t *spi)
{
    (void)spi;
    /* TODO: read JEDEC ID, configure */
    return ${NAME_UPPER}_OK;
}
C_EOF
}

generate_c_none() {
    cat > "$WHEEL_DIR/${NAME_LOWER}.c" << C_EOF
#include "${NAME_LOWER}.h"

/* TODO: implement */
C_EOF
}

# ---- CMakeLists.txt ----------------------------------------------------------

cat > "$WHEEL_DIR/test/CMakeLists.txt" << CMAKE_EOF
cmake_minimum_required(VERSION 3.13)
project(test_${NAME_LOWER} C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_${NAME_LOWER}
    test_${NAME_LOWER}.c
    ../${NAME_LOWER}.c
)

target_include_directories(test_${NAME_LOWER} PRIVATE
    \${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(test_${NAME_LOWER} PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME ${NAME_LOWER}_tests COMMAND test_${NAME_LOWER})
CMAKE_EOF

# ---- Test file ---------------------------------------------------------------

generate_test_i2c() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../${NAME_LOWER}.h"

/* ---------- mock I2C ---------- */

#define MOCK_REG_COUNT 256
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

#define WRITE_LOG_MAX 32
static struct { uint8_t reg; uint8_t val; } write_log[WRITE_LOG_MAX];
static int write_log_idx;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    write_log_idx   = 0;
    /* TODO: set default register values (chip ID, status flags, etc.) */
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    if (reg < MOCK_REG_COUNT) mock_regs[reg] = val;
    if (write_log_idx < WRITE_LOG_MAX) {
        write_log[write_log_idx].reg = reg;
        write_log[write_log_idx].val = val;
        write_log_idx++;
    }
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = ((reg + i) < MOCK_REG_COUNT) ? mock_regs[reg + i] : 0xFF;
    return true;
}

static const ${NAME_LOWER}_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \\
    tests_run++; \\
    if (!(cond)) { \\
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \\
        tests_fail++; \\
    } \\
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEQ(a, b, msg)  ASSERT((a) != (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a)-(b)) < (tol), msg)

/* ---------- tests ---------- */

static void test_init_ok(void)
{
    printf("test_init_ok\n");
    mock_reset();
    ${NAME_LOWER}_err_t e = ${NAME_LOWER}_init(&mock_i2c);
    ASSERT_EQ(e, ${NAME_UPPER}_OK, "init returns OK");
}

static void test_init_i2c_fail(void)
{
    printf("test_init_i2c_fail\n");
    mock_reset();
    mock_fail_at = 0;
    ${NAME_LOWER}_err_t e = ${NAME_LOWER}_init(&mock_i2c);
    ASSERT_EQ(e, ${NAME_UPPER}_ERR_I2C, "init fails on I2C error");
}

/* TODO: add more tests */

int main(void)
{
    test_init_ok();
    test_init_i2c_fail();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
TEST_EOF
}

generate_test_spi() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include <stdio.h>
#include <string.h>
#include "../${NAME_LOWER}.h"

/* ---------- mock SPI ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;
static uint8_t mock_cs_state;

static uint8_t xfer_log[256];
static int     xfer_log_idx;

static void mock_reset(void)
{
    mock_fail_next = 0;
    mock_op_count  = 0;
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
    /* TODO: return appropriate mock data */
    return 0xFF;
}

static const ${NAME_LOWER}_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- test helpers ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \\
    tests_run++; \\
    if (!(cond)) { \\
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \\
        tests_fail++; \\
    } \\
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEQ(a, b, msg)  ASSERT((a) != (b), msg)

/* ---------- tests ---------- */

static void test_init_ok(void)
{
    printf("test_init_ok\n");
    mock_reset();
    ${NAME_LOWER}_err_t e = ${NAME_LOWER}_init(&mock_spi);
    ASSERT_EQ(e, ${NAME_UPPER}_OK, "init returns OK");
}

/* TODO: add more tests */

int main(void)
{
    test_init_ok();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
TEST_EOF
}

generate_test_none() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include <stdio.h>
#include "../${NAME_LOWER}.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \\
    tests_run++; \\
    if (!(cond)) { \\
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \\
        tests_fail++; \\
    } \\
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)

/* TODO: add tests */

int main(void)
{
    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
TEST_EOF
}

# ---- README ------------------------------------------------------------------

# Map layer to level label
case "$LAYER" in
    primitives) LAYER_LABEL="L0 (primitives)" ;;
    drivers)    LAYER_LABEL="L1 (drivers)" ;;
    protocols)  LAYER_LABEL="L2 (protocols)" ;;
    modules)    LAYER_LABEL="L3 (modules)" ;;
    _draft)     LAYER_LABEL="draft" ;;
    *)          LAYER_LABEL="$LAYER" ;;
esac

generate_standard_readme() {
    cat > "$WHEEL_DIR/README.md" << README_EOF
# ${NAME_LOWER} — TODO: one-line description

**Status: candidate**
**Layer: ${LAYER_LABEL}**

TODO: 2-3 sentence description.

## Why not VSF native?

Searched \`vsf/source/service/\`, \`vsf/source/component/\`, \`vsf/source/utilities/\` for:
\`TODO: keywords\` — TODO: results.

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

## Tests

\`\`\`bash
cd wheels/${LAYER}/${NAME_LOWER}/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_${NAME_LOWER}
\`\`\`

## Test status

- Unit tests (sim): TODO
- HIL: N/A

## Convention Deviations

None.

## API review

- **Date**: TODO
- **Key decisions**: TODO
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
    # TODO: add per-project config header dir if needed
)
# Note: -Wall -Wextra -Wpedantic omitted for vendor wheels (upstream code triggers warnings)

enable_testing()
add_test(NAME ${NAME_LOWER}_tests COMMAND test_${NAME_LOWER})
CMAKE_EOF
}

generate_vendor_test() {
    cat > "$WHEEL_DIR/test/test_${NAME_LOWER}.c" << TEST_EOF
#include <stdio.h>
#include <string.h>

/* TODO: #include upstream header */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \\
    tests_run++; \\
    if (!(cond)) { \\
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \\
        tests_fail++; \\
    } \\
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)

/* TODO: add tests */

int main(void)
{
    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
TEST_EOF
}

generate_vendor_readme() {
    cat > "$WHEEL_DIR/README.md" << README_EOF
# ${NAME_LOWER} — TODO: one-line description

**Status: candidate**

TODO: 2-3 sentence description.

## Why not VSF native?

Searched \`vsf/source/service/\`, \`vsf/source/component/\`, \`vsf/source/utilities/\` for:
\`TODO: keywords\` — TODO: results.

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

## Porting (per-project config)

TODO: describe per-project config files if applicable.

## Convention Deviations

TODO: document any deviations from api-conventions.md.

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| TODO   | TODO    | TODO    |

## Known issues

TODO.

## Supported platforms

- [x] Host (sim) — TODO: date
- [ ] TODO: real hardware target — not yet verified

## Tests

\`\`\`bash
cd wheels/${LAYER}/${NAME_LOWER}/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_${NAME_LOWER}
\`\`\`

## Test status

- Unit tests (sim): TODO
- HIL: N/A

## API review

- **Date**: TODO
- **Key decisions**: TODO
README_EOF
}

# ---- Dispatch generation -----------------------------------------------------

if [[ "$VENDOR" -eq 1 ]]; then
    generate_vendor_dirs
    generate_vendor_md
    generate_vendor_test_cmake
    generate_vendor_test
    generate_vendor_readme
else
    case "${HAL:-none}" in
        i2c)  generate_header_i2c;  generate_c_i2c;  generate_test_i2c  ;;
        spi)  generate_header_spi;  generate_c_spi;   generate_test_spi  ;;
        *)    generate_header_none; generate_c_none;   generate_test_none ;;
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
echo "=== CI: add this line to the wheel-test matrix in .github/workflows/ci.yml ==="
echo ""
# Determine test directory path
if [[ "$VENDOR" -eq 1 ]] || [[ -d "$WHEEL_DIR/test" ]]; then
    CI_DIR="wheels/${LAYER}/${NAME_LOWER}/test"
else
    CI_DIR="wheels/${LAYER}/${NAME_LOWER}"
fi
echo "          - { name: ${NAME_LOWER}, dir: ${CI_DIR} }"
echo ""

echo "Done. Next steps:"
if [[ "$VENDOR" -eq 1 ]]; then
echo "  1. Copy upstream headers to vendor/inc/ and sources to vendor/src/"
echo "  2. Fill in VENDOR.md (upstream URL, version, license, file list)"
echo "  3. Fill in TODO placeholders in README.md"
echo "  4. Wire vendor sources in test/CMakeLists.txt"
echo "  5. Add tests to test/test_${NAME_LOWER}.c"
else
echo "  1. Fill in TODO placeholders in ${NAME_LOWER}.h / ${NAME_LOWER}.c"
echo "  2. Add tests to test/test_${NAME_LOWER}.c"
fi
echo "  $(( VENDOR ? 6 : 3 )). Add the matrix entry above to ci.yml"
echo "  $(( VENDOR ? 7 : 4 )). Run: tooling/scripts/test_all.sh ${NAME_LOWER}"
echo "  $(( VENDOR ? 8 : 5 )). Run: python tooling/scripts/audit_wheels.py"
