# Poor Man's Sky

## Why this experiment?

Could we squeeze a little No Man's Sky-style wonder out of a laptop well over a
decade old, with a GPU measuring VRAM in **megabytes, not gigabytes**? That's the
experiment: take a Pentium M, a Radeon RV350 with **64 MiB VRAM**, and **2 GiB RAM**,
then see how far procedural generation, streaming and old-school graphics tricks
can take us.

With RAM feeling like a luxury again, digging out an old Acer seemed like a
perfectly reasonable response. The ambition is a tiny explorable universe;
the research question is how much wonder fits into very little memory.
**Space is vast. Our VRAM isn't.**

An experimental planetary exploration demo written in C99 and GLSL 1.20,
trying to get the most out of a Pentium M, a Radeon RV350 with 64 MiB VRAM,
and 2 GiB RAM. Built and tested primarily on Debian i686/X11.

Explore a spherical planet and a smaller, reachable Moon: procedural terrain,
vegetation, oceans, clouds, atmosphere, day/night and lunar phases. The renderer
uses adaptive terrain patches, LOD morphing, vegetation batching/impostors,
compressed texture pages, local sun shadows and water reflections. Planet and
Moon share disk/RAM/VRAM streaming infrastructure. This is a work in progress;
old-driver stability and visual artifacts remain areas for improvement.

## Build and run

On the original Acer (vendored SDL2 headers, installed SDL2/OpenGL libraries):

```sh
make
./run.sh
```

On macOS, install SDL2 with Homebrew (`brew install sdl2`), then run
`./run.sh`. The launcher detects macOS, builds with Clang and the OpenGL
framework, and opens a window. Both Apple Silicon and Intel Homebrew prefixes
are discovered through `sdl2-config` or `brew`. Linux keeps the Acer build and
display setup above.

The Moon's orbit is oriented at startup so it appears above the horizon to the
right of the default starting view; the player spawn and heading are unchanged.
The Moon continues its orbit, and `--time` / `--moon-phase` still change its position.

The launcher starts `bin/poor-mans-sky` from `bin/`. You can also run:

```sh
cd bin
./poor-mans-sky
```

On Linux, default presentation is exclusive fullscreen at 640×480. `+` and `-` cycle the
available X display modes. Use `--windowed` for a window. F1 lists controls;
F2 toggles the HUD, F3 wireframe, F4 cycles Low → Medium → High → Low, F5 sun shadows,
F6 advances time, F8 clouds, and F12 captures a screenshot.

### Quality presets

Medium/Balanced is the default. F4 cycles all three presets; the profiler shows
both the level and its purpose. Start directly with `./run.sh --preset low`
(or `medium` / `high`); `--performance` is an alias for Low.

| Setting | Low / Performance | Medium / Balanced | High / Quality |
| --- | --- | --- | --- |
| Internal resolution at 640×480 | 480×360 | 640×480 | 640×480 |
| Terrain detail range, walk / fly | 8 / 24 km | 16 / 48 km | 32 / 96 km |
| Near detail bubble, walk / fly | 80 / 240 m | 100 / 300 m | 140 / 420 m |
| Vegetation / rocks range | 700 m | 1,400 m | 2,500 m |
| Grass fade interval | 18–30 m | 25–42 m | 30–50 m |
| Reflections | off | 64², every frame | 128², every frame |
| Local shadow map | 256² | 512² | 512² |
| Post-processing | single texture sample | bloom 128² | bloom 256², vignette, dithering |

Low also uses the simple terrain material and lower mesh, vegetation LOD and
cloud budgets. Medium and High keep full terrain materials nearby and the
existing simple shader beyond the microdetail range. Shaders already cheap
remain shared; quality changes their workload instead of duplicating programs.

All presets preserve the 360° near bubble, expanded camera cone, distance bands,
conservative occlusion and coarse planet coverage. The terrain range limits
**refinement**, so the horizon and planet remain visible beyond it, including
from orbit. F4 reuses procedural caches and streams the newly requested detail.
Feature toggles and `--no-*` switches still apply; Low disables reflection/bloom
passes even when their master switches are enabled. `--reflection-interval`
overrides the cadence in Medium/High. Presets do not change the world's seed or
asset identities. Settings live in `src/quality-presets.h`.

Walk: WASD and mouse, Space/right mouse for the jetpack, E/F to board/land.
Xbox flight: left stick pitch/roll, right stick camera orbit (click to reset),
RT/LT progressive forward/reverse, LB/RB yaw, X brake, Y land, hold A superboost.
The complete current mapping is in the F1 help dialog.

The default Makefile is tuned for the Acer's 32-bit Pentium M. For another Linux
machine, install SDL2/OpenGL development packages and override the flags:

```sh
make CC=cc CFLAGS='-std=c99 -O2 -Wall -Wextra' LIBS='-lSDL2 -lGL -lm -ldl -lpthread'
```

Python 3 generates the small procedural source textures during the build.
No image packages or asset downloads are required. `make clean` removes only
executables, preserving cache and captures.

## Cache and offline generation

Both executables use **`./cache` relative to their launch working directory**.
`--cache-dir PATH` overrides this. Assets/shaders are resolved relative to the
executable's `bin/` directory; do not move the binary away from the source tree.
Screenshots go to `./screenshots` in the launch directory.

```sh
make baker
cd bin
./poor-mans-sky-baker --threads 1 --mib 512
```

On a Mac with Homebrew SDL2, `make baker-mac` builds the same offline generator.
Run it from `bin/` with `--threads 8 --mib 512`, then transfer its `cache/` to the
runtime machine while neither program is writing that cache. Keep the target's
existing cache: do not overwrite overlapping regional packs blindly. Native
startup generation remains available for machines without a pregenerated cache.

Cache files contain versioned, checksummed regional packs, including GPU-ready
compressed texture payloads. Generator/asset changes invalidate affected domains;
rendering and input edits should not. The first startup builds missing data.
`bin/`, caches, binaries, diagnostics and captures are excluded from Git.

## Layout and checks

- `src/poor-mans-sky.c`, `src/*.h`: runtime and procedural generators.
- `src/poor-mans-sky-baker.c`: offline cache generation.
- `src/cache-compat.json`: verified compatibility aliases for generated data.
- `shaders/`: RV350-compatible rendering shaders.
- `assets/`: source materials and provenance.
- `tools/`: procedural material generator and focused regression checks.
- `third_party/SDL2/`: SDL2 headers with their original license notices.

Run `python3 tools/check-runtime-cache.py` to check cache isolation. This test
briefly edits/restores source files; do not run it concurrently with a build.
Additional CPU and OpenGL checks are documented in `tools/README.md`.

## License

Original code, shaders and project documentation: **Mozilla Public License 2.0**
(see [LICENSE](LICENSE)). MPL is file-level copyleft: distributed modifications
to covered files remain available under MPL, while separate code can use other
licenses. See the [Mozilla FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/).

Third-party headers retain their existing licenses. Asset origin and scope are
listed in [THIRD_PARTY.md](THIRD_PARTY.md). No No Man's Sky assets are included;
this project is independent of Hello Games.
