#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Builds the real port sources, unmodified, against the host stand-ins in host/
# and runs each model. Usage: run.sh [flash|i2c|spi|usart|gpio ...]
# The sources are copied to a scratch tree because they include their vendor
# headers by relative path, and a developer's tree may hold the real ones.
set -u

T=$(cd "$(dirname "$0")" && pwd)
P=$(dirname "$T")
B=$(mktemp -d "${TMPDIR:-/tmp}/gd32f4xx_host.XXXXXX")
trap 'rm -rf "$B"' EXIT

CFLAGS="-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter
        -Wno-int-to-pointer-cast -fno-omit-frame-pointer
        -fsanitize=address,undefined -fno-sanitize-recover=undefined"

mkdir -p "$B/common/vendor/Include" "$B/hal/arch" "$B/utilities"
cp "$P/common/common.h" "$B/common/"
cp "$T"/host/vendor/*.h "$B/common/vendor/Include/"
: > "$B/__device.h"
: > "$B/hal/arch/vsf_arch.h"
: > "$B/utilities/ooc_class.h"

rc=0
[ $# -gt 0 ] || set -- flash i2c spi usart gpio
for p in "$@"; do
    [ -f "$T/test_$p.c" ] || { echo "$p: no model yet"; continue; }
    up=$(echo "$p" | tr '[:lower:]' '[:upper:]')
    mkdir -p "$B/common/$p" "$B/hal/driver/common/$p"
    cp "$P/common/$p/$p.c" "$P/common/$p/$p.h" "$B/common/$p/"
    echo "VSF_${up}_CFG_IMP_LV0(0, )" > "$B/hal/driver/common/$p/${p}_template.inc"
    # shellcheck disable=SC2086
    if ! gcc $CFLAGS ${HOST_CFLAGS:-} -DHOST_USE_$up -I"$B" -I"$T/host" \
            "$B/common/$p/$p.c" "$T/host/host.c" "$T/test_$p.c" -o "$B/test_$p"; then
        echo "$p: BUILD FAILED"
        rc=1
        continue
    fi
    "$B/test_$p" || rc=1
done
[ $rc -eq 0 ] && echo "ALL MODELS PASS" || echo "MODEL FAILURES"
exit $rc
