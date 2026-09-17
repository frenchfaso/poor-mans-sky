# SPDX-License-Identifier: MPL-2.0
.DEFAULT_GOAL := all
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Ithird_party -march=pentium-m -mtune=pentium-m -O3 -mfpmath=sse -ffast-math -fomit-frame-pointer
LIBS = /usr/lib/i386-linux-gnu/libSDL2-2.0.so.0 /usr/lib/i386-linux-gnu/libGL.so.1 -lm -ldl -lpthread
HEADERS := $(wildcard src/*.h)

.PHONY: all demo materials clouds baker baker-mac clean run FORCE
all: bin/poor-mans-sky
demo: bin/poor-mans-sky
baker: bin/poor-mans-sky-baker
bin:
	mkdir -p bin
bin/poor-mans-sky: src/poor-mans-sky.c $(HEADERS) src/cache-build.h assets/cloud-procedural.rgba | bin
	$(CC) $(CFLAGS) src/poor-mans-sky.c -o $@ $(LIBS)
bin/poor-mans-sky-baker: src/poor-mans-sky-baker.c src/poor-mans-sky.c $(HEADERS) src/cache-build.h assets/cloud-procedural.rgba | bin
	$(CC) $(CFLAGS) src/poor-mans-sky-baker.c -o $@ $(LIBS)
clouds: assets/cloud-procedural.rgba
assets/cloud-procedural.rgba: tools/generate-clouds.py
	python3 tools/generate-clouds.py
	touch $@
materials: FORCE
	python3 tools/generate-materials.py
src/cache-build.h: materials
	python3 tools/cache-fingerprint.py
FORCE:
# No cache, assets or saved captures are deleted by clean.
clean:
	rm -f bin/poor-mans-sky bin/poor-mans-sky-baker
run: bin/poor-mans-sky
	cd bin && ./poor-mans-sky
baker-mac: src/cache-build.h | bin
	clang -std=c99 -O3 -ffp-contract=off -Wno-deprecated-declarations -I/opt/homebrew/include -L/opt/homebrew/lib src/poor-mans-sky-baker.c -o bin/poor-mans-sky-baker -lSDL2 -framework OpenGL -lm -lpthread
