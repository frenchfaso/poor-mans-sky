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

`check-render-targets.c`: tutte le 18 combinazioni risoluzione interna/preset, completezza FBO, formato4:3, drawable invariato e limiti dei tasti. `check-quality-cycle.c` verifica anche i tasti +/−, inclusi quelli del tastierino, durante i cicli F4, mantenendo invariati modalità video e contesto GL. `--still` ignora gli eventi di input: non usarlo per i test dei tasti.

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

`make clouds`: generate three orthogonal views of each of four cloud volumes offline.
The 256x256 RGBA atlas consumes 256 KiB of VRAM. Run `python3 tools/check-cloud-atlas.py` to check its format, opacity
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

## Flight rendering

`check-flight-rendering.c`: compile like the OpenGL checks and run from `bin/`.
Checks actual depth-test occlusion of the third-person ship near either body's
surface, visibility despite distant near clipping in space, foreground alpha
blending, lighting based on the final camera, and shadow-cache invalidation for
roll, boarding state, and a 24-angle orbit with a fixed shadow footprint. The ship shares the nearest body's depth projection
near the surface; the reserved foreground range is used only in clear space.
Clouds are composed after opaque actors. No GPU readback is added to the runtime.


`check-cloud-roll.c`: OpenGL check (compile as above and run from `bin/`).
Reads the cloud vertex buffer across 24 roll angles: world-space geometry,
UVs, color and opacity must remain identical, including after compound yaw/pitch
loops. Also checks the three orthogonal projection weights. Planet-fixed cards
blend according to the camera position, with no orientation history. One draw
and one texture sample per fragment; projected overlap retains its area budget.

## View-directed streaming (polygonal engine)

The default policy keeps a 360-degree bubble (walk 100 m, fly 300 m), then uses
an expanded camera frustum with distance bands at 4×, 16× and 64× that radius.
The expansion is 10 degrees per side; already wanted detail uses 16 degrees and
15% distance hysteresis. Off-cone terrain keeps a coarse covering mesh.
Terrain pages, generation/disk requests, RAM preloading and GPU uploads share
this policy. Vegetation keeps its species/LOD ranges but filters and prioritizes
cells against the same view. Cached cells survive turns; existing LOD fades remain.

`--legacy-streaming` selects the previous full-surround policy for comparisons.
`check-stream-view.c` is a CPU regression (compile like `check-lod-rings.c`):
bubble, distance/FOV boundaries, conservative bounds, yaw/roll, fly range,
request/upload ordering and coarse-parent fallback. `check-lod-rings.c` and
`check-ram-plan.c` also cover the legacy fallback. Camera snapshots are published
under the terrain mutex; render culling uses its own tighter frustum.

Streaming hot paths compare squared distances, reuse normalized FOV coefficients
until the viewport changes, and reuse planes until orientation changes. Queue
scores are refreshed once per dispatch under the mutex; popping jobs only compares
stored scores. Upload priorities are calculated once per item before sorting.
The policy regression compares 20,000 cases against the square-root reference
and checks replacement in a full wrapped queue.

The compact profiler also runs on main: CPU process time (including workers),
current RSS, asynchronous GPU time when supported, Radeon global VRAM at 2 Hz
or an allocation estimate. The RV350 reports GPU timing as unavailable. Graphs
use triangle strips, preserving the workaround for native-line driver lockups.

`check-quality-cycle.c`: compile like the OpenGL checks, then run from `bin/`:

```sh
./check-quality-cycle --windowed --frames 210 --preload --ram-preload-mib 0 --no-vsync
```

Exercises two complete F4 cycles through real SDL key events, with terrain and
vegetation workers active, in walk and fly. Checks coarse planet coverage,
all six internal sizes, fixed output/context, shader selection, bloom targets,
reflection state and shadow sizes. The internal resolution survives F4 changes.
Do not pass `--still` (it ignores keys) or a non-default startup preset.
`check-stream-view.c` also checks distance limits/hysteresis for all three presets
and both movement modes; `check-flight-rendering.c` checks shadow map resizing
and the matching receiver texel step.

## Lunar rendering and lighting

`check-lunar-policy.c` is a CPU check (compile like `check-stream-view.c`). It
checks planet/Moon solar occultation, penumbra, lunar bubble/cone priorities,
all presets in walk/fly, cached planning and coarse coverage in a full queue.

`check-lunar-rendering.c` is an OpenGL integration check:

```sh
./check-lunar-rendering --moon --moon-phase 0.68 --still --frames 360 --ram-preload-mib 0 --no-vsync
```

It checks disjoint lunar coverage with live workers, rapid turns, all presets,
ship shadow receivers and an invariant shadow footprint while orbiting the
flight camera. Use `--windowed` on Mac; run from `bin/`. Lunar detail uses the
same streaming bubble, expanded cone and distance bands as planetary terrain.
Occlusion results are asynchronous and invalidated by camera/body/mesh changes.
The ship's direct light uses analytic planet/Moon occultation; emissive and
ambient light remain independent. Local ship shadows fade above 100 m AGL.
