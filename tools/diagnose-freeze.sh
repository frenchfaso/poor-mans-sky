#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Explicit diagnostic run; fsync checkpoints affect performance measurements.
set -eu
cd "$(dirname "$0")/../bin"
case_name=${1:-baseline}
case "$case_name" in
 baseline) set -- ;;
 no-shadows) set -- --no-sun-shadows ;;
 no-reflections) set -- --no-reflections ;;
 no-queries) set -- --no-occlusion ;;
 no-clouds) set -- --no-clouds ;;
 no-warmup) set -- --no-shader-warmup ;;
 *) echo 'Cases: baseline no-shadows no-reflections no-queries no-clouds no-warmup' >&2; exit 2 ;;
esac
mkdir -p diagnostics
stem="diagnostics/$(date +%Y%m%d-%H%M%S)-$case_name"
printf 'Diagnostic logs: %s.log and %s.trace\n' "$stem" "$stem"
exec ./poor-mans-sky --windowed --resolution 640x480 --still --preload --ram-preload-mib 0 --frames 120 --trace-gpu "$stem.trace" "$@" > "$stem.log" 2>&1
