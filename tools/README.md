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

## Flight rendering

`check-flight-rendering.c`: compile like the OpenGL checks and run from `bin/`.
Checks actual depth-test occlusion of the third-person ship near either body's
surface, visibility despite distant near clipping in space, foreground alpha
blending, lighting based on the final camera, and shadow-cache invalidation for
roll, boarding state, and a 24-angle orbit with a fixed shadow footprint. The ship shares the nearest body's depth projection
near the surface; the reserved foreground range is used only in clear space.
Clouds are composed after opaque actors. No GPU readback is added to the runtime.
