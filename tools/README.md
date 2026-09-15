## Cache v3 e formati compatti

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
