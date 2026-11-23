# tk64 — Ultimate Tapan Kaikki for the Commodore 64

A cc65 demake of the game, built from `../linuxTK` data with `tools/conv64.c`.
See `c64portplan.md` for the design and the current status (section 13).

```sh
make            # converter, assets, program -> build/tk64.d64 (DEFAULT episode)
make run        # start in VICE (x64sc)
make test       # headless VICE run with a scripted joystick, screenshot build/test.png
make test-2p    # same in split screen mode, screenshot build/test2p.png
make test-kill  # same, with two enemies placed in front of the player
make test-close # an enemy overlapping the player, punched with the fist
make test-shop  # the shop screen (SHOPTEST build), screenshot build/testshop.png
make size       # program segment sizes
EPISODES=12 make   # convert more episodes (digits = episode numbers)
```

On the title screen press `1` (or fire) for one player, `2` for two players in
split screen. Joystick left/right (or A/D, F1) on the title screen picks the difficulty:

| | enemy energy | damage taken from enemies |
|---|---|---|
| EASY | half | half |
| NORMAL | as in the original | as in the original |
| HARD | as in the original | x1.5 |
| GOD | as in the original | none (no damage from anything) |

One player HUD: selected weapon (yellow), its ammo (cyan, red under 10, `---` for
the fist) and energy (green, yellow, red) on the first row; message, enemies left,
cash and time on the second; all weapons on the third, owned ones white, the
selected one yellow with `>`. Split screen shows weapon, ammo and energy per player.

Shop: before every level after the first, each player gets a shop screen, as in
the PC version: 11 weapons, 9 ammo types and the shield (each shield level adds 100
energy). Up/down select, fire or right buys, the weapon key (Q, C=, right SHIFT) or
left sells, RUN/STOP or DONE leaves. Weapons sell for a random 30-80 % of the price,
ammo for its full price. White = affordable, blue = too expensive, green = full.

In a one player game every joystick and key set controls the player.
In a two player game:

| | walk | back | turn left | turn right | fire | next weapon |
|---|---|---|---|---|---|---|
| Player 1: joystick port 2 | up | down | left | right | fire | – |
| Player 1: keys | W | S | A | D | left SHIFT | Q (or C=) |
| Player 2: joystick port 1 | up | down | left | right | fire | – |
| Player 2: keys | - | @ | P | * | + | right SHIFT |

Working two player combinations: two joysticks; joystick 2 + player 2 keys; both key
sets. (Player 1 keys + joystick 1 does not work, see below.)

The 12 keys are chosen so that no three of them form a rectangle in the C64
keyboard matrix: any combination can be held without ghost (shadow) keys. Player 2's
keys are on matrix lines PA5–PA7, which a joystick in port 2 does not disturb, so
"joystick + keyboard" two-player works. A joystick in port 1 garbles the keyboard rows,
so the keyboard is ignored while it is pressed. RUN/STOP (alone) quits to the title.

Split screen is top/bottom: two 40x12 character views with one shared HUD row between
them (player 1 weapon, ammo, energy | enemies left | player 2). This direction suits the
VIC-II: a raster interrupt in the HUD row reprograms all 8 hardware sprites, so each
view has its own 8 (own player, the other player, 4 enemies, a grenade, an
explosion), and the map routine keeps drawing whole 40-char rows. A left/right split
would need both views to share the 8 sprites, because sprites are multiplexed by
raster line. Players can hurt each other, as in the original. The level is lost when
both players are dead; a dead player returns at the next level.

Map colours: `make CONV64_PAL=0,11,15` (background, $D022, $D023). Tiles use only these
three; the colour RAM colour is red and used only for blood, gibs and crates.

Needs cc65 (tested 2.18), VICE (3.10), and a C compiler and libpng for the converter.

Graphics: editable PNG sheets in `gfx/`, used instead of the converted graphics when
valid and regenerated when missing or broken. See `GRAPHICS.md`.
