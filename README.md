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
F2 toggles the HUD, F3 wireframe, F4 performance mode, F5 sun shadows,
F6 advances time, F8 clouds, and F12 captures a screenshot.

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

## Experimental CPU terrain caster

The `codex/voxel-terrain-experiment` branch provides an opt-in spherical
heightfield caster inspired by [VoxelSpace](https://github.com/s-macke/VoxelSpace)
and [Grégory Massal's terrain article](https://www.massal.net/article/voxel/).

```sh
./run.sh --voxel-terrain
./run.sh --voxel-terrain --voxel-scale 2
```

The experimental mode streams immediately by default. Add `--preload` to wait
for the full startup/RAM preload; `--no-preload` still explicitly skips it.
Large disk caches show an interactive indexing screen before scene startup.

The CPU reads caster-specific height/normal arrays and decoded colors, then
produces base color, normals and depth; a GLSL 1.20 pass lights/composites it.
The experimental path never prepares adaptive terrain mesh LODs, morphs,
stitching, texture fades, sorting, batches or terrain occlusion queries, even
when the caster is unavailable. The streamed heightfield cover and water grids
remain separate; their quadtree is still needed for data selection.

The fallback is one static, welded cube sphere: 32 cells per face edge,
6,146 shared vertices, 12,288 triangles, 196,648 GPU bytes. It is generated and
uploaded once per world, with vertex colors from the existing material/climate
fields. There are no terrain patch VBOs in this path. Reflections reuse this
same mesh. Shadows retain existing casters, with a depth-based receiver for
the caster and a static-mesh receiver for the fallback. Vegetation, actors,
water, clouds, sky and the Moon remain enabled. The legacy renderer without
`--voxel-terrain` remains available for comparison; lunar LOD is unchanged.

Between 1,500 and 2,000 metres above the reference sphere, complementary pixel
masks progressively replace the caster with the static mesh (also between 55°
and 58° looking down). Returning reverses the same smoothstep fade. Both paths
run in this band, even when stationary; the dither can be visible. Rapid camera
jumps can switch abruptly to keep invalid caster views off screen. The mesh
has kilometre-scale cells: nearby terrain, coastlines, vegetation placement and
collision height can disagree with its coarse surface. These experimental
thresholds/resolution are a performance tradeoff, not a close-up quality guarantee.

An exact cache key reuses caster payloads and skips GPU uploads when camera,
projection, resolution, selected cover and payload revision are unchanged.
Lighting and other objects still update. Cover bounds, unchanged water topology,
Hi-Z data and quantized transition masks also reuse cached results.
`--voxel-no-cache` disables raster reuse for timing comparisons; do not present
stationary cache-hit timings as moving-camera performance. `--voxel-check`
compares cached payloads with fresh raw-page rasterization every 120 frames.

The terrain depth also supplies a CPU Hi-Z hierarchy for occlusion of vegetation
cells/groups in the main view. It preserves sky holes and the farthest depth,
uses padded bounds, and never reuses visibility in reflections or shadow maps.
Hi-Z is disabled during the mixed transition because caster depth alone no
longer describes all terrain pixels.
After 30 unsuccessful frames it pauses for 120 frames, resuming on camera motion.
Use `--voxel-no-hiz` for comparison. `--voxel-hiz-check` verifies every rejected
box against GPU occlusion queries; this stalls and is only for validation.

Other optimizations include front-to-back Y-buffer occlusion, distance/patch
steps, coherent lookup, visible-sample shading data and conservative patch/horizon
skipping. `--voxel-scale 2` quarters terrain buffer pixels and upload bytes while
the rest of the scene keeps its normal resolution, trading terrain detail for speed.

This remains experimental. Constant-depth spans and texture LOD transitions
can still show artifacts, and the static mesh omits small terrain features.
The three RGBA8 payloads add 3.52 MiB of VRAM at 640×480 (0.88 MiB at scale 2).
The CPU page cache is bounded by residency slots (about 53 MiB maximum), allocated
on demand. Hi-Z begins with 8×8 depth blocks and uses about 47 KiB at 960×600.
The dedicated path has been tested on Mac; Acer performance must be remeasured.
See `tools/README.md` for regression checks.

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
