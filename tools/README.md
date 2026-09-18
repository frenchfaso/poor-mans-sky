## Cache v3 e formati compatti

`check-terrain-detail.c`: verifica la selezione conservativa dello shader senza
microdettaglio e confronta 12 rendering OpenGL con lo shader completo (due
varianti di nebbia, dissolvenza texture e tre distanze). Compilare come gli altri
test OpenGL ed eseguire dalla radice del progetto. Il limite è 3 livelli su 255
di errore massimo e meno di 0,5 livelli medi nel framebuffer a 8 bit.

`check-morph-uploads.c`: compilare come gli altri test OpenGL, poi eseguire
`python3 tools/check-morph-uploads.py /tmp/check-morph-uploads` (su Acer con
`DISPLAY=:0`). Confronta upload immediati e differiti su 14 pose deterministiche:
lettura dei VBO, bounds e LOD identici, massimo un upload per tassello/frame.
Copre raffinamento, semplificazione, rimozione delle giunzioni e uscita/rientro
nella selezione. I contatori e l'orologio fisso sono presenti solo nel test.

`check-body-seams.c` verifica anche il riuso delle griglie marine: confronta i
vertici con l'algoritmo precedente (`water-reference.h`), coprendo cambi LOD,
modifiche dei vicini, riuso degli slot, riordinamento della selezione e ritorno
alla stessa geometria. Richiede un contesto OpenGL; compilare come `check-suite.c`.
Il buffer di riferimento aggiuntivo è presente soltanto nel test.
Verifica anche il riuso delle giunzioni del terreno a geometria invariata,
il riordinamento dei tasselli e la ricostruzione dopo un cambio LOD.

`check-packed.c`: dimensioni ABI, round-trip, limiti di lettura, checksum corrotto, recupero e limite/espulsione degli archivi; confronto dell’errore BC1 su 1000 blocchi. Passare una directory temporanea nuova.

`check-portable.c CACHE_DIR`: legge 18 livelli intorno allo spawn da una cache pregenerata e confronta posizioni e altezze con il generatore locale. Richiede la cache Mac completa: fallisce con meno di 15 hit, errori di altezza >= 0,5 m o posizione >= 1 m.

`check-index.c`: confronta le 225 celle indicizzate con il generatore float, incluse le tolleranze di quantizzazione.

`check-render-targets.c`: dieci combinazioni dimensione/qualità, FBO e dimensioni drawable. I cambi fullscreen reali vanno verificati separatamente sull’hardware destinatario; `--still` ignora gli eventi di input, quindi non usarlo per i test dei tasti.

Build headless Mac dei test CPU: `clang -std=c99 -O2 -ffp-contract=off -Wno-deprecated-declarations -I/opt/homebrew/include -L/opt/homebrew/lib tools/check-packed.c -o /tmp/check-packed -lSDL2 -framework OpenGL -lm -lpthread`.

Verifiche su Acer / Debian i686, dalla cartella principale del progetto.

Suite CPU + OpenGL (richiede il display dell’Acer):

```sh
gcc -std=c99 -Ithird_party -march=pentium-m -O1 tools/check-suite.c -o /tmp/check-poor-mans-sky-suite /usr/lib/i386-linux-gnu/libSDL2-2.0.so.0 /usr/lib/i386-linux-gnu/libGL.so.1 -lm -ldl -lpthread
DISPLAY=:0 /tmp/check-poor-mans-sky-suite
```

Dissolvenze BC1, inclusa lettura dei dati sulla GPU:

```sh
gcc -std=c99 -Ithird_party -march=pentium-m -O1 tools/check-textures.c -o /tmp/check-poor-mans-sky-textures /usr/lib/i386-linux-gnu/libSDL2-2.0.so.0 /usr/lib/i386-linux-gnu/libGL.so.1 -lm -ldl -lpthread
DISPLAY=:0 /tmp/check-poor-mans-sky-textures
```

Invalidazione selettiva della cache e mancata ricompilazione senza modifiche:

```sh
python3 tools/check-cache-domains.py
```

Quest’ultimo test modifica temporaneamente due sorgenti e li ripristina nel blocco `finally`: eseguirlo in una copia isolata, senza compilazioni concorrenti. Le suite usano directory di cache temporanee.

`check-ram-plan.c` verifica precaricamento iniziale 512 MiB e pianificazione del pool, simmetria della priorità, ordinamento della coda e capacità del pool. Compilare come gli altri test, sostituendo il nome del file. Per verificare due avvii senza attendere il primo riempimento completo: `--still --preload --ram-preload-mib 64 --frames 20`; il secondo deve leggere dal disco le pagine preparate dal primo.

## Procedural assets

`make clouds`: generate the four cloud density/lighting projections offline.
The 128x128 RGBA atlas consumes the same 64 KiB of VRAM as the former startup
texture. Run `python3 tools/check-cloud-atlas.py` to check its format, opacity
and transparent padding against the octagonal runtime mesh. The generator
uses only Python's standard library and runs again only
when its source changes. Runtime builds require the generated asset.

Experimental periodic cellular rock material (default materials unchanged):

```sh
python3 tools/generate-materials.py --variant cellular --output-dir diagnostics/materials-cellular
```

Inspect the images before adopting them: changed material bytes invalidate
terrain, nature and foliage cache domains. The experimental command requires
a separate output directory to keep comparisons isolated.

`check-reflection-shader.c`: compile like the OpenGL tests and run from the
repository root. Compares the old shared shader (`reflection-reference.frag`)
with the specialized opaque/reflection programs in 12 cases: daytime/nighttime
lighting, fog, and planes that clip all, some or none of the geometry. Limits:
3/255 maximum and less than 0.5/255 mean error. Also run `check-terrain-detail.c`
for the CPU fog and texture transition variants.

## Experimental CPU terrain caster

`check-voxel-terrain.c` is a headless CPU check: compile like `check-packed.c`.
It checks cube-face boundaries, coherent lookup against a root traversal,
BC1 decoding/cache invalidation, compact CPU payload equivalence and proxy sampling.

For a live scene, `./run.sh --voxel-check --still --frames 360 --no-preload`
compares the compact CPU payloads with raw-page sampling, with and without hierarchical/horizon skipping
every 120 frames. A mismatch terminates the run. Repeat with `--altitude 100`,
`--altitude 1000 --pitch -0.5` and `--view 2 --voxel-scale 2`.
This diagnostic includes a second rasterization and must not be used for timing.
It checks skipping against the same column caster, not against the mesh renderer.


`check-voxel-pipeline.c` requires OpenGL. Compile like the other GL checks.
It verifies proxy VBO contents/reuse, memory accounting, single-build static mesh reuse,
return to the caster without patch VBOs/morphs, complementary GLSL stipple coverage/depth at 25/50/75%,
and Hi-Z with a known occluder/sky hole plus GPU audit.
Place the executable in `bin/` and run from there with `--live` to exercise
an actual scene switching caster → high-altitude mesh → mixed band → caster →
vertical mesh view → caster over 260 frames. Camera changes are confined to its
input wrapper. `--altitude 400000 --pitch -1.45` checks orbital fallback;
`--altitude 1730 --pitch -0.5` exercises the mixed band. Altitude CLI values are
AGL, whereas renderer thresholds are relative to the reference sphere.

`./run.sh --voxel-hiz-check --frames 480 --still --no-preload --no-vsync`
audits each Hi-Z rejected box using GPU sample queries. Compare timing with
`--voxel-terrain` and `--voxel-terrain --voxel-no-hiz`, never with audit enabled.
Check `VOXEL_HIZ` counts: zero rejected boxes means no culling benefit in that
pose, even if the run passes. Reflections and shadow casters are excluded.


`check-static-planet.c` is a CPU test (build like `check-voxel-terrain.c` in
`bin/`). It checks all triangle indices, outward winding, two triangles per
undirected edge, shared cube borders and Euler characteristic 2. It loads source
materials but does not create an OpenGL context. The raster test also checks
exact cache-key invalidation for camera, projection, resolution and payloads.
Use `--voxel-no-cache` to measure raster work without stationary reuse;
`VOXEL_CACHE` reports rebuild/reuse counts and static mesh allocations.


`check-cache-saturation.c` is a CPU check (compile like `check-voxel-terrain.c`).
It uses an eight-entry index, fills it, attempts 10,000 further insertions and
checks that saturation does not trigger repeated whole-table compactions.
It also verifies existing-key updates, retirement, reclamation and revival.
For startup regressions, run `./run.sh --voxel-terrain --frames 120` without
`--still` or `--no-preload`: these previously masked the normal startup path.

## Experimental persistent hybrid terrain

`./run.sh --voxel-hybrid` replaces the old `--voxel-gpu-rays` backend. Terrain
samples are uploaded once per resident block (an 8 MiB LRU cache, 16-byte
vertices with locally quantized positions). GLSL 1.20
projects grid lines into downward curtains; hardware depth chooses the visible
surface. No per-pixel CPU/GPU ray search, CPU-generated screen spans, framebuffer
readback, or static mesh underneath the near terrain. The static welded planet
remains for distant views, crossfade and reflections. Other scene objects remain
active; shadows reuse the curtains with an equal-depth receiver pass.

This is an approximation: depth and relief are sampled along grid lines, with
possible banding and orientation changes. It requires visual/hardware evaluation.
`--voxel-terrain` remains the CPU reference. `--voxel-scale` only controls that CPU
backend; hybrid uses scene resolution. CPU Hi-Z is inactive for hybrid; normal
GPU depth testing remains active. `HYBRID` reports persistent builds/reuse,
evictions, geometry and bytes. `TERRAIN_SURFACE` must report zero main static draws
when mix is 1. Benchmark complete streamed scenes, not the first few frames.

`check-voxel-hybrid.c` (OpenGL) checks analytic plane coverage/depth, draw-order
independence, index bounds, immutable sample contents, reuse on camera/stride
changes, slot invalidation and memory accounting. Compile like the other GL
checks. `check-voxel-pipeline --live-hybrid`, run from `bin/`, exercises actual
caster → far mesh → blend → caster → vertical fallback → caster transitions and
asserts that no static surface is drawn under the full caster.

## Performance HUD and startup

F2 toggles the minimal HUD: large FPS, a 20-second CPU/GPU time graph and current
RAM/GPU-memory graphs. CPU is process CPU time per frame, including workers and
excluding waits. GPU is asynchronous elapsed render time, excluding the HUD and
swap; unsupported/unavailable timers show N/A. These overlapping times are not
percent utilization and should not be added. No blocking query readback is used.
RAM is current process RSS (including driver mappings); GPU EST is engine-counted
allocations against the 56 MiB target budget, not physical driver residency.

One monotonic bar spans indexing, geology, materials, shaders, scene preload and
optional RAM preload. Its weights indicate completed phases, not predicted time.
Cached, generated and skipped phases share this same bar. The geology UI change
preserves payload identities via `cache-compat.json`; generators are unchanged.

`check-performance-ui.c` (OpenGL) checks monotonic progress, GL state restoration
with a deleted shader, current RSS, history wrap and unavailable GPU timing. Build
like other GL checks and run from the repository root or `bin/`.
