# linuxTKeditor - level editor for Ultimate Tapan Kaikki, C89 core + raylib backend

CC       ?= cc
CORE_CFLAGS = -std=c89 -pedantic -Wall -Wextra -O2
PLAT_CFLAGS = -std=c99 -Wall -Wextra -O2
LIBS     = -lraylib -lm

CORE = src/main.c src/data.c src/level.c src/gfx.c src/editor.c
CORE_OBJ = $(CORE:src/%.c=build/%.o)

all: tked

tked: $(CORE_OBJ) build/plat_raylib.o
	$(CC) -o $@ $^ $(LIBS)

# headless build with scripted input (see src/plat_test.c), no raylib needed
tked_test: $(CORE_OBJ) build/plat_test.o
	$(CC) -o $@ $^

build/plat_raylib.o: src/plat_raylib.c src/plat.h src/types.h | build
	$(CC) $(PLAT_CFLAGS) -c $< -o $@

build/plat_test.o: src/plat_test.c src/plat.h src/types.h | build
	$(CC) $(PLAT_CFLAGS) -c $< -o $@

build/%.o: src/%.c src/ed.h src/types.h src/plat.h | build
	$(CC) $(CORE_CFLAGS) -c $< -o $@

build:
	mkdir -p build

# loads and saves every level of the game and compares the files
test: tked_test tools/roundtrip
	./tools/roundtrip ..

tools/roundtrip: tools/roundtrip.c build/level.o build/data.o build/gfx.o build/plat_test.o
	$(CC) $(CORE_CFLAGS) -Isrc -o $@ $^

clean:
	rm -rf build tked tked_test tools/roundtrip

.PHONY: all test clean
