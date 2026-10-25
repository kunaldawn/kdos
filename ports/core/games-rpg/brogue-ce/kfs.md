## About

[Brogue CE]{.idx}, the community edition of Brogue, is a roguelike of 26 dungeon levels, played
for the Amulet of Yendor, with a small set of rules and a lot of interaction between monsters,
terrain and items. This port builds only its graphical front end, which draws with SDL2 either as
tiles or as glyphs; the game is `brogue`.

## Flags

- `SDL_CONFIG='pkg-config sdl2'` — the command the Makefile runs for SDL2's compile and link flags.
  Its default is `sdl2-config`, which [sdl2-compat](#port-sdl2-compat) does not install, so
  `pkg-config sdl2` answers instead with the same `--cflags` and `--libs` arguments.
- `DATADIR=/usr/share/brogue-ce` — compiled in as the directory the game reads its assets and
  key map from. The default is `.`, the directory the program is run from.
- `TERMINAL=NO` — with `GRAPHICS=YES`: the SDL2 front end only, without the ncurses one for a
  terminal. Both are upstream's defaults, named.
- `RELEASE=YES` — a release build. Otherwise the Makefile runs `tools/git-extra-version`, which,
  with no git repository to ask, appends `-dev` to the version string the game shows.

## Notes

Brogue writes its saves, recordings and high scores into its working directory. Started from the
menu that would be the home directory, or `/` for a launcher that sets none, so the program
itself installs as `/usr/libexec/brogue-ce/brogue`, and the `brogue` in `/usr/bin` is a small
script written by `build.sh` that creates `${XDG_DATA_HOME:-$HOME/.local/share}/brogue-ce`,
changes into it and starts the game there.

The Makefile has no install target, so `build.sh` installs the program, the key map, the assets,
the icon and the licence by hand. The executable keeps the name `brogue`, which is SDL's default
Wayland app id, so `StartupWMClass=brogue` matches the window although the command the entry runs
is the script.
