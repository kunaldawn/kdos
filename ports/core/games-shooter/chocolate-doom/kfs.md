## About

[Chocolate Doom]{.idx} is a Doom source port that aims to behave exactly like the original DOS
executables, bugs and limits included, and plays Heretic, Hexen and Strife on the same code. It
installs `chocolate-doom`, `chocolate-heretic`, `chocolate-hexen`, `chocolate-strife`, a setup
program for each, and `chocolate-server`. With [freedoom](#port-freedoom) in `depends`, the free
Freedoom IWADs are in `/usr/share/games/doom`, one of the directories the engine searches, so
`chocolate-doom` has a game to play without any of id Software's data.

## Flags

- `-g0` — appended to `CFLAGS`: no debug information. `configure.ac` sets
  `CFLAGS="-O2 -g <warnings> <your CFLAGS>"` for GCC, so the build's own flags come after its `-g`,
  and only a `-g0` among them cancels it (see the
  [rule on debug information](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `--enable-sdl2mixer` — with `--enable-sdl2net`: music and sound effects through
  [sdl2-mixer](#port-sdl2-mixer), and network games through [sdl2-net](#port-sdl2-net). Both are
  on by default, named.
- `--enable-doc` — with `--enable-bash-completion`: build the manual pages and the Bash
  completions, which upstream generates with Python from the source. Each is on by default and
  quietly turned off, with only a warning, when `python3` is missing.
- `--enable-fonts` — with `--enable-icons`: regenerate, from their PNG sources, the fonts of the
  text-mode screens (the setup program, the ENDOOM screen, the network game's waiting screen) and
  the window icons compiled into each program, with Python and
  [python3-pillow](#port-python3-pillow). The same quiet fallback applies.
- `--with-libsamplerate` — with `--with-libpng` and `--with-fluidsynth`: resample sound effects
  with [libsamplerate](#port-libsamplerate), write screenshots as PNG, and play MIDI music through
  [fluidsynth](#port-fluidsynth). Each defaults to `check`, which builds without the library when
  it is missing; named, a missing one fails the configure.
- `--disable-werror` — do not add `-Werror`. Upstream adds it only for `--enable-werror`, so this
  names the default.
- `appdatadir=/usr/share/metainfo` — on the `make install` line. The release's makefiles have no
  variable of this name; they install the AppStream files through `metainfodir`, which is already
  `/usr/share/metainfo`, so the assignment changes nothing.

## Notes

The source is GitHub's archive of the release tag, which carries `configure.ac` but no generated
`configure`, so `build.sh` runs `autoreconf -fi` first; [autoconf](#port-autoconf) and
[automake](#port-automake) are in `depends` for it.

Upstream installs a desktop entry for each of the four games, the setup program and a Doom
screensaver. `build.sh` removes the Heretic, Hexen and Strife entries, whose games need data KDOS
does not ship (their programs stay, for a player with the data), and writes the Doom and setup
entries itself with `StartupWMClass` set to each program's name, which is its Wayland app id.
