## About

[Crispy Doom]{.idx} is a limit-removing Doom source port built on Chocolate Doom: it keeps the
original's look and behaviour but renders at higher resolution, raises the engine's static limits
so larger maps load, and adds quality-of-life features. Like Chocolate Doom it plays Heretic, Hexen
and Strife too, and it installs `crispy-doom`, `crispy-heretic`, `crispy-hexen`,
`crispy-strife`, a setup program for each, and `crispy-server`. With
[freedoom](#port-freedoom) in `depends`, the Freedoom IWADs in `/usr/share/games/doom` give
`crispy-doom` a game to play.

## Flags

- `-g0` — appended to `CFLAGS`: no debug information. `configure.ac` sets
  `CFLAGS="-O2 -g <warnings> <your CFLAGS>"` for GCC, so the build's own flags come after its `-g`,
  and only a `-g0` among them cancels it (see the
  [rule on debug information](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `--enable-sdl2mixer` — with `--enable-sdl2net`: music and sound effects through
  [sdl2-mixer](#port-sdl2-mixer), and network games through [sdl2-net](#port-sdl2-net). Both are
  on by default, named.
- `--enable-doc` — with `--enable-bash-completion`: build the manual pages and the Bash
  completions, which upstream generates with Python. Each is on by default and quietly turned off,
  with only a warning, when `python3` is missing.
- `--enable-fonts` — with `--enable-icons`: regenerate the setup program's fonts and the programs'
  icons from their PNG sources, with Python and [python3-pillow](#port-python3-pillow). The same
  quiet fallback applies.
- `--with-libsamplerate` — with `--with-libpng` and `--with-fluidsynth`: resample sound effects
  with [libsamplerate](#port-libsamplerate), write screenshots as PNG, and play MIDI music through
  [fluidsynth](#port-fluidsynth). Each defaults to `check`, which builds without the library when
  it is missing; named, a missing one fails the configure.
- `--with-zlib` — read maps whose nodes are stored compressed, in ZDBSP's compressed format, with
  [zlib](#port-zlib). Like the three above, it defaults to `check`, and naming it makes zlib
  required.
- `--enable-truecolor` — render in true colour rather than through Doom's 256-colour palette,
  which upstream marks experimental and leaves off by default.
- `--disable-werror` — do not add `-Werror`. Upstream adds it only for `--enable-werror`, so this
  names the default.
- `appdatadir=/usr/share/metainfo` — on the `make install` line. The release's makefiles have no
  variable of this name; they install the AppStream files through `metainfodir`, which is already
  `/usr/share/metainfo`, so the assignment changes nothing.

## Notes

The source is GitHub's archive of the release tag, which carries `configure.ac` but no generated
`configure`, so `build.sh` runs `autoreconf -fi` first.

Upstream installs a desktop entry for each of the four games, the setup program and a Doom
screensaver. `build.sh` removes the Heretic, Hexen and Strife entries, whose games need data KDOS
does not ship (their programs stay, for a player with the data), and writes the Doom and setup
entries itself with `StartupWMClass` set to each program's name, which is its Wayland app id.
