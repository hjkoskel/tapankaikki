# tk64 - Ultimate Tapan Kaikki for the Commodore 64 (cc65)
#   make            converter + assets + build/tk64.d64
#   make run        start in VICE
#   make test       autoplay build, run headless in VICE, save build/test.png
#   make test-2p    same in split screen mode, save build/test2p.png
#   make size       segment sizes of the program

TKDATA   ?= ..
EPISODES ?= 1
# map colours: background, $$D022, $$D023 (gore is always red in colour RAM)
CONV64_PAL ?= 0,11,15
export CONV64_PAL
# graphics override PNGs (GRAPHICS.md): used when valid, (re)generated otherwise
CONV64_GFX ?= gfx
export CONV64_GFX
GFXPNG   = $(wildcard $(CONV64_GFX)/*.png)
CL65     = cl65
CFLAGS   = -t c64 -Oirs -Cl -I src -I build
LTK      = ../linuxTK/src
SHOT     ?= build/test.png
CYCLES   ?= 30000000

SRC  = src/main.c src/game.c src/data64.c src/tables.c build/gfxdata.c src/map.s src/irq.s src/ovl.s

all: build/tk64.d64

build/conv64: tools/conv64.c $(LTK)/assets.c $(LTK)/globals.c $(LTK)/data.c $(LTK)/rout.c $(LTK)/plat_test.c
	mkdir -p build
	cc -O2 -Wall -o $@ $^ -lpng

build/gfxdata.c: build/conv64 $(GFXPNG)
	mkdir -p $(CONV64_GFX)
	./build/conv64 $(TKDATA) build $(EPISODES)

build/tk64.prg: $(SRC) src/defs.h build/gfxdata.c c64.cfg
	$(CL65) $(CFLAGS) -C c64.cfg -m build/tk64.map -o $@ $(SRC)

TESTDEFS ?= -DAUTOPLAY
build/tk64test.prg: $(SRC) src/defs.h build/gfxdata.c c64.cfg FORCE
	$(CL65) $(CFLAGS) $(TESTDEFS) -C c64.cfg -o $@ $(SRC)

FORCE:

# disk image: program, graphics and all converted levels
DISKFILES = -write build/tkchr.prg tkchr -write build/tksprd.prg tksprd -write build/tkspre.prg tkspre \
            $(foreach l,$(sort $(wildcard build/l*.prg)),-write $(l) $(basename $(notdir $(l))))

build/tk64.d64: build/tk64.prg build/gfxdata.c
	c1541 -format "tapan kaikki,64" d64 $@ -write build/tk64.prg tk64 $(DISKFILES) >/dev/null

build/tk64test.d64: build/tk64test.prg build/gfxdata.c
	c1541 -format "tapan kaikki,64" d64 $@ -write build/tk64test.prg tk64 $(DISKFILES) >/dev/null

run: build/tk64.d64
	x64sc -autostart build/tk64.d64

# headless test: emulate CYCLES cycles in warp mode, then save a screenshot
test: build/tk64test.d64
	timeout 120 x64sc -silent -sounddev dummy -warp +drive8truedrive -trapdevice8 -limitcycles $(CYCLES) \
	  -exitscreenshot $(SHOT) -autostart build/tk64test.d64 >/dev/null 2>&1 || true
	@ls -l $(SHOT)

# shop screen (normal build + SHOPTEST), screenshot build/testshop.png
test-shop:
	$(MAKE) test TESTDEFS=-DSHOPTEST SHOT=build/testshop.png

# split screen test: two players, both follow the script
test-2p:
	$(MAKE) test TESTDEFS="-DAUTOPLAY -DAUTOPLAY_2P" SHOT=build/test2p.png

# point blank test: an enemy overlapping the player, killed with the fist
test-close:
	$(MAKE) test TESTDEFS="-DAUTOPLAY -DAUTOPLAY_CLOSE" CYCLES=20000000 SHOT=build/close.png

# combat test: two enemies placed in front of the player
test-kill:
	$(MAKE) test TESTDEFS="-DAUTOPLAY -DAUTOPLAY_KILL" CYCLES=14000000

size: build/tk64.prg
	@awk '/^Segment list:/{f=1} f && /^(CODE|RODATA|DATA|BSS|ZEROPAGE|STARTUP|ONCE|INIT) /{printf "%-9s %6d bytes\n", $$1, strtonum("0x"$$4)}' build/tk64.map

clean:
	rm -rf build

.PHONY: all run test test-2p test-close test-kill size clean FORCE
