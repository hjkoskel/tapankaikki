# linuxTK - Ultimate Tapan Kaikki port, C89 core + raylib backend

CC       ?= cc
CORE_CFLAGS = -std=c89 -pedantic -Wall -Wextra -O2
PLAT_CFLAGS = -std=c99 -Wall -Wextra -O2
LIBS     = -lraylib -lm

CORE = src/main.c src/globals.c src/data.c src/assets.c src/gfx.c src/rout.c \
       src/effect.c src/bullet.c src/player.c src/enemy.c src/level.c \
       src/render.c src/screens.c src/game.c
PLAT = src/plat_raylib.c

CORE_OBJ = $(CORE:src/%.c=build/%.o)
PLAT_OBJ = $(PLAT:src/%.c=build/%.o)

all: tk size

tk: $(CORE_OBJ) $(PLAT_OBJ)
	$(CC) -o $@ $^ $(LIBS)

build/plat_raylib.o: src/plat_raylib.c src/plat.h src/types.h | build
	$(CC) $(PLAT_CFLAGS) -c $< -o $@

# headless build with scripted input (see src/plat_test.c), no raylib needed
tk_test: $(CORE_OBJ) build/plat_test.o
	$(CC) -o $@ $^ -lm

build/plat_test.o: src/plat_test.c src/plat.h src/types.h | build
	$(CC) $(CORE_CFLAGS) -c $< -o $@

build/%.o: src/%.c src/globals.h src/data.h src/types.h src/plat.h | build
	$(CC) $(CORE_CFLAGS) -c $< -o $@

build:
	mkdir -p build

# static memory footprint of the game core (text/data/bss)
size: $(CORE_OBJ)
	@size -t $(CORE_OBJ) | tail -1 | awk '{printf "core memory: text %d, data %d, bss %d bytes\n", $$1, $$2, $$3}'

# the core must not use the heap
check:
	@! grep -nE '\b(malloc|calloc|realloc|free)\s*\(' $(CORE) || (echo "heap use found in core"; false)
	@echo "no heap use in core"

clean:
	rm -rf build tk tk_test gore_test

.PHONY: all size check clean

# visual gore/effects test (headless)
gore_test: $(filter-out build/main.o,$(CORE_OBJ)) build/plat_test.o tools/gore_test.c
	$(CC) $(CORE_CFLAGS) -o $@ tools/gore_test.c $(filter-out build/main.o,$(CORE_OBJ)) build/plat_test.o -lm
