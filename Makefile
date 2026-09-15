# SPDX-License-Identifier: MPL-2.0
.DEFAULT_GOAL := all
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Ithird_party -march=pentium-m -mtune=pentium-m -O3 -mfpmath=sse -ffast-math -fomit-frame-pointer
LIBS = /usr/lib/i386-linux-gnu/libSDL2-2.0.so.0 /usr/lib/i386-linux-gnu/libGL.so.1 -lm -ldl -lpthread
HEADERS := $(wildcard *.h)

.PHONY: all demo materials baker baker-mac clean run FORCE
all: bin/poor-mans-sky
demo: bin/poor-mans-sky
baker: bin/poor-mans-sky-baker
bin:
	mkdir -p bin
bin/poor-mans-sky: poor-mans-sky.c $(HEADERS) cache-build.h | bin
	$(CC) $(CFLAGS) poor-mans-sky.c -o $@ $(LIBS)
bin/poor-mans-sky-baker: poor-mans-sky-baker.c poor-mans-sky.c $(HEADERS) cache-build.h | bin
	$(CC) $(CFLAGS) poor-mans-sky-baker.c -o $@ $(LIBS)
materials: FORCE
	python3 tools/generate-materials.py
cache-build.h: materials
	python3 tools/cache-fingerprint.py
FORCE:
# No cache, assets or saved captures are deleted by clean.
clean:
	rm -f bin/poor-mans-sky bin/poor-mans-sky-baker
run: bin/poor-mans-sky
	cd bin && ./poor-mans-sky
baker-mac: cache-build.h | bin
	clang -std=c99 -O3 -ffp-contract=off -Wno-deprecated-declarations -I/opt/homebrew/include -L/opt/homebrew/lib poor-mans-sky-baker.c -o bin/poor-mans-sky-baker -lSDL2 -framework OpenGL -lm -lpthread
