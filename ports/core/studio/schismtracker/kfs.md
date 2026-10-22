## About

Schism Tracker is a re-creation of Impulse Tracker: it composes and plays IT, S3M, XM and MOD
music in Impulse Tracker's text-mode interface, which it draws itself on [SDL3](#port-sdl3). The
recipe builds from upstream's dated tag archive, and the date is the version.

## Flags

- `--without-x` — no X11 code. configure looks for X with `AC_PATH_X` and, when it finds libX11,
  builds its X11 clipboard and event code (`sys/x11/`) for an SDL window on X11; SDL programs on KDOS open
  Wayland windows (the session sets `SDL_VIDEODRIVER=wayland`, see
  [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `--with-sdl3` — with `--without-sdl2` and `--without-sdl12`: SDL3 is the one video and audio
  back end. The tree's SDL2 and SDL 1.2 are [sdl2-compat](#port-sdl2-compat) and
  [sdl12-compat](#port-sdl12-compat), which run on the same SDL3.
- `--enable-sdl3-linking` — and every other `--enable-*-linking` switch (ALSA, FLAC, libavformat,
  zlib, bzip2, lzma, zstd): link the library at build time instead of upstream's default, which
  is to `dlopen()` it at run time when it is there. Linked, each library is a `NEEDED` entry of the
  program, so the package's dependencies are the ones the loader sees.
- `--with-alsa` — ALSA, Schism's own MIDI back end.
- `--without-jack` — no JACK support.
- `--with-flac` — and `--with-avformat`: FLAC samples, and reading samples in the formats
  libavformat ([ffmpeg](#port-ffmpeg)) decodes.
- `--with-zlib` — and `--with-bzip2`, `--with-lzma`, `--with-zstd`: open files compressed with
  gzip, bzip2, xz or zstd.
- `--disable-tests` — do not build upstream's test program.

## Patches

- `version.patch` — In `configure.ac`, the `m4_esyscmd` that computes the version (the date of the last git commit, through `git log`) prints `$SCHISM_VERSION` instead when it is set. `build.sh` runs `autoreconf` with `SCHISM_VERSION=$version`. The tag archive is not a git checkout, so without the patch `git log` prints nothing and the program reports an empty version.

## Notes

The tag archive carries no `configure` script; `autoreconf -fi` writes it, and that is the moment
the version is read.

The recipe replaces upstream's `schism.desktop` and its `/usr/share/pixmaps` icon with a
`schismtracker.desktop` and upstream's PNG icons under `hicolor` in the program's name. SDL3 names
the window after the program, `schismtracker`, which the entry repeats in `StartupWMClass`.
Upstream's entry claims the IT, S3M, XM and MOD types; the replacement claims none.
