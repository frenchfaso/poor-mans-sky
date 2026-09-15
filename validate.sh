#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")"
mkdir -p bin/screenshots
./run.sh --frames 240 --view 1 --tour --no-vsync > bin/screenshots/stream-check.log 2>&1
if grep -Ei 'dummy shader|compiler error|cannot handle|FATAL:|GL error|software rasterizer' bin/screenshots/stream-check.log; then exit 1; fi
tail -1 bin/screenshots/stream-check.log
