#!/bin/bash
# Thin wrapper around the generic build_flash_check.sh, fed with flybox.conf.
# All actual orchestration logic lives in build_flash/build_flash_check.sh.
#
# Usage (unchanged from before refactor):
#   ./flybox_build_flash.sh              # build + flash + check
#   ./flybox_build_flash.sh build        # build only
#   ./flybox_build_flash.sh flash        # flash only
#   ./flybox_build_flash.sh check [sec]  # serial check only

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/build_flash/build_flash_check.sh" \
    --conf "$SCRIPT_DIR/build_flash/flybox.conf" "$@"
