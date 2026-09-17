# Repository Guidelines

## Project Structure & Module Organization

Poor Man’s Sky is a C99/GLSL 1.20 planetary exploration demo targeting Debian i686/X11, a Pentium M, and 64 MiB VRAM.

- `src/poor-mans-sky.c` is the runtime; subsystem implementations live in `src/*.h`.
- `src/poor-mans-sky-baker.c` generates offline caches.
- `shaders/` contains rendering shaders; `assets/` holds source materials and provenance.
- `tools/` contains procedural generators and regression checks; `third_party/SDL2/` contains vendored headers.
- `bin/` contains executables and normal launch-time output and is ignored by Git.

## Build, Test, and Development Commands

- `make`: generate procedural materials/cache fingerprints and build the runtime using Acer-specific flags and libraries.
- `./run.sh`: detect Mac/Linux, build and launch from `bin/`; Mac uses Clang/Homebrew SDL2 and a window. On Linux, add `--windowed` for a window.
- `make baker`: build the offline generator; `make baker-mac` supports Homebrew SDL2 under `/opt/homebrew`.
- `make clean`: remove executables while preserving caches and captures.
- `./validate.sh`: run a 240-frame rendering tour and check its log for graphics failures; requires a working display.

For another Linux machine with SDL2/OpenGL development packages:

```sh
make CC=cc CFLAGS='-std=c99 -O2 -Wall -Wextra' LIBS='-lSDL2 -lGL -lm -ldl -lpthread'
```

## Coding Style & Naming Conventions

Follow surrounding C style: two-space indentation, opening braces on the same line, camelCase function/variable names, and uppercase macro constants. Use descriptive hyphenated filenames such as `terrain-field.h`. Preserve MPL SPDX headers. No formatter or linter is configured; review compiler warnings. Keep shaders compatible with GLSL 1.20 and account for the target’s memory limits.

## Testing Guidelines

Tests are standalone C checks (`tools/check-*.c`) and Python scripts, with no configured coverage threshold. See `tools/README.md` for compilation commands and CPU/OpenGL requirements. Add focused checks for changed behavior and verify fullscreen/driver behavior on target hardware.

Run `python3 tools/check-runtime-cache.py` for cache isolation and `python3 tools/check-cache-domains.py` for selective invalidation. These temporarily modify sources: use an isolated checkout without concurrent builds.

## Commit & Pull Request Guidelines

Recent commits use short, imperative descriptions, such as “Move engine sources and headers into src.” Follow that style. PRs should explain behavior changes, list validation and hardware used, link relevant issues, and include screenshots for visual changes.

## Cache & Generated Data

Caches resolve relative to the launch directory unless `--cache-dir` overrides it. Preserve existing caches; avoid concurrent writers. Let build tools regenerate `src/cache-build.h`. Generator changes should invalidate affected domains; rendering/input edits should preserve compatibility.

## Local Agent Knowledge

When present, start with `wiki/START-HERE.md` for task routing, measured performance, implementation invariants, and self-contained technical recipes. The wiki is local and Git-ignored; verify dated notes against current source and keep new measurements distinct from proposals. Do not force-add wiki files to Git.
