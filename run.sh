#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")"
case "$(uname -s)" in
    Darwin)
        if command -v sdl2-config >/dev/null 2>&1; then
            sdl_config=$(command -v sdl2-config)
        elif command -v brew >/dev/null 2>&1; then
            sdl_config="$(brew --prefix sdl2)/bin/sdl2-config"
        else
            echo 'SDL2 not found. Install Homebrew SDL2: brew install sdl2' >&2
            exit 1
        fi
        if [ ! -x "$sdl_config" ]; then
            echo 'SDL2 not found. Run: brew install sdl2' >&2
            exit 1
        fi
        sdl_prefix=$("$sdl_config" --prefix)
        make CC=clang \
            CFLAGS="-std=c99 -O3 -ffp-contract=off -Wall -Wextra -Wno-deprecated-declarations -I\"$sdl_prefix/include\"" \
            LIBS="$("$sdl_config" --libs) -framework OpenGL -lm -lpthread"
        set -- --windowed "$@"
        ;;
    Linux)
        export DISPLAY="${DISPLAY:-:0}"
        if [ -d "/run/user/$(id -u)" ]; then
            export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
            export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}"
        fi
        make
        ;;
    *)
        echo "Unsupported platform: $(uname -s) (expected macOS or Linux)" >&2
        exit 1
        ;;
esac
cd bin
exec ./poor-mans-sky "$@"
