# Ultimate Tapan Kaikki

**TK – Tapan Kaikki** is a top-down action game for DOS by Error Free Productions.
This repository holds the original game with its source code, plus new
ports of the game and of its level editor:

| Folder / file | What it is |
|---|---|
| `GAME.EXE`, `EDITOR.EXE`, `SETUP.EXE`, `SRC/` | the original DOS game, level editor and Watcom C++ source |
| [`linuxTK/`](linuxTK/README.md) | the game ported to plain C and [raylib](https://www.raylib.com/) for Linux |
| [`linuxTKeditor/`](linuxTKeditor/README.md) | the level editor `EDITOR.EXE` ported to C and raylib |
| [`tk64/`](tk64/README.md) | a Commodore 64 version of the game (cc65), with split screen two player mode |
| [`editor.html`](https://hjkoskel.github.io/tapankaikki/editor.html) | a level editor that runs in the browser and shows the whole map on screen |
| `EFPS/`, `LEVS/`, `WAVS/`, `MUSIC/`, `FNTS/`, `PALETTE.TAB` | the game data that all versions use |

**[Open the level editor in your browser](https://hjkoskel.github.io/tapankaikki/editor.html)**

## Quick start

```sh
cd linuxTK && make && ./tk                 # the game on Linux (needs raylib)
cd linuxTKeditor && make && ./tked         # the level editor on Linux
cd tk64 && make && make run                # the C64 version in VICE (needs cc65 and VICE)
python3 -m http.server                     # then open http://localhost:8000/editor.html
```

The browser editor reads the graphics and levels from this repository, so it works
on GitHub Pages as is. It needs to be served over http; opening it from disk
(`file://`) does not work. Levels are saved as downloads; put them in
`LEVS/<EPISODE>/LEVELn.LEV` to play them.

## Documentation

- `OLD_README.TXT`, `EDITOR.TXT`, `EPISODES.TXT`, `WHAT'S.NEW` – the original documents
- `PortToC.md` – the plan for the C port
- `MemoryUsageReport.md` – memory use of the C port
- `tk64/c64portplan.md` – the design of the C64 version
- `tk64/GRAPHICS.md` – how the C64 graphics are made, for artists

## Credits

The original game, from `OLD_README.TXT`:

- Main programming: Allu
- Additional programming: Unski
- Music: Raipe, Allu & Unski
- Graphics: Allu
- Audio system: MIDAS Digital Audio System v1.xx
- VESA 2: Vertigo

Error Free Productions: Unski (coder, musician, Internet expert) and Allu (coder,
musician, GFX guru).

The Linux and Commodore 64 ports, the Linux level editor and the browser level
editor: porting work directed by **Henri Koskela**.

## License

GNU General Public License version 3, see [`LICENSE`](LICENSE).
