## About

[Ironwail]{.idx} is a Quake engine from the QuakeSpasm line, with a rewritten OpenGL renderer
that keeps the original game's look while drawing large modern maps quickly. The game data is not
free and is not shipped: a player puts their own `pak0.pak` (and `pak1.pak`) in
`~/.ironwail/id1`, one of the places the engine searches and the directory the desktop entry's
comment names.

## Flags

- `SDL_CONFIG='pkg-config sdl2'` — the command the Makefile runs for SDL2's compile and link flags.
  Its default is `sdl2-config`, which [sdl2-compat](#port-sdl2-compat) does not install.
- `DO_USERDIRS=1` — with `USE_SDL2=1`: keep configuration, saves and screenshots in `~/.ironwail`
  rather than the game directory, and build on SDL2. Both are upstream's defaults, named.
- `USE_CURL=0` — build without cURL, which upstream uses only to download the official add-ons of
  the Quake re-release, from the re-release's own add-on server, into the in-game mods menu.
  [curl](#port-curl) is not in `depends`.
- `USE_CODEC_WAVE=1` — with `USE_CODEC_FLAC=1`, `USE_CODEC_MP3=1`, `USE_CODEC_VORBIS=1`,
  `USE_CODEC_OPUS=1`, `USE_CODEC_XMP=1` and `USE_CODEC_UMX=1`: play streamed music as WAV, FLAC
  ([flac](#port-flac)), MP3, Ogg Vorbis ([libvorbis](#port-libvorbis)), Opus
  ([opusfile](#port-opusfile)), tracker modules ([libxmp](#port-libxmp)) and Unreal UMX. FLAC,
  Opus, XMP and UMX are off upstream.
- `MP3LIB=mpg123` — with `VORBISLIB=vorbis`: decode MP3 with [mpg123](#port-mpg123) rather than
  libmad, and Ogg with libvorbis rather than Tremor. Both are upstream's defaults, named.
- `USE_CODEC_MIKMOD=0` — with `USE_CODEC_MODPLUG=0`: tracker modules go through libxmp alone; the
  Makefile takes one of the three module libraries.
- `STRIP=true` — make the Makefile's strip step a no-op. A release build runs `strip` on the
  program, and the tree keeps symbols in every program of this size, so a backtrace names its
  functions (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`build.sh` first runs `make -C Misc/pak`, which packs the engine's own menu graphics lumps and its
`default.cfg` into `ironwail.pak` with upstream's `mkpak.sh`, and copies the result over the
prebuilt `Quake/ironwail.pak` in the source before the engine is built.

The engine finds `ironwail.pak` beside the path it was started by, so the program and the pak are
installed together in `/usr/lib/ironwail`, and `/usr/bin/ironwail` is a small script that starts
the program by its full path; a symbolic link would leave the engine looking in `/usr/bin`. The
window's Wayland app id is the executable's name, `ironwail`.
