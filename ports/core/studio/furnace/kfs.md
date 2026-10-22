## About

Furnace is a chiptune tracker: it composes music for the sound chips of old consoles, computers
and arcade machines, emulating each chip, and exports to audio and to the chips' own register-log
formats. Its interface is Dear ImGui drawn on SDL2 ([sdl2-compat](#port-sdl2-compat)), with
OpenGL or SDL's own renderer, so it links no toolkit.

## Flags

- `-DBUILD_GUI=ON` — build the tracker itself, not only the headless player.
- `-DSYSTEM_SDL2=ON` — and `SYSTEM_FMT`, `SYSTEM_FFTW`, `SYSTEM_FREETYPE`, `SYSTEM_LIBSNDFILE`,
  `SYSTEM_PORTAUDIO`, `SYSTEM_RTMIDI`, `SYSTEM_ZLIB`: take each library from its port instead of
  the copy under `extern/`, which is left unbuilt. Upstream's default is the vendored copy for
  every one of them.
- `-DUSE_SDL2=ON` — and `USE_SNDFILE`, `USE_RTMIDI`, `USE_FREETYPE`, `WITH_PORTAUDIO`: SDL2, which
  the interface requires; libsndfile for reading and writing audio files; MIDI through
  [rtmidi](#port-rtmidi) over ALSA; FreeType for font rendering; audio out through
  [portaudio](#port-portaudio) as well as SDL.
- `-DWITH_JACK=OFF` — no JACK back end. Upstream's default turns it on whenever pkg-config finds
  `jack`, which the [pipewire](#port-pipewire) port installs, so it is named off.
- `-DWITH_LOCALE=OFF` — no translations through libintl; the package is English only under the
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
  `-DUSE_MOMO=OFF` leaves out momo, the libintl replacement upstream builds where the system has
  none.
- `-DUSE_BACKWARD=OFF` — no backward-cpp crash backtraces. backward-cpp needs `<execinfo.h>`,
  which musl does not have; upstream's default already checks for that header.
- `-DWITH_RENDER_SDL=ON` — and `WITH_RENDER_OPENGL`, `WITH_RENDER_OPENGL1`: build the
  SDL_Renderer back end and both OpenGL back ends, so a machine with no usable OpenGL still draws
  through SDL. `-DUSE_GLES=OFF` uses desktop OpenGL rather than OpenGL ES.
- `-DWITH_DEMOS=OFF` — do not install the demo songs, which upstream notes are not under the GPL.
  `-DWITH_INSTRUMENTS=ON` and `-DWITH_WAVETABLES=ON` install the instrument and wavetable
  libraries.
- `-DWARNINGS_ARE_ERRORS=OFF` — warnings in Furnace's own C++ do not stop the build.

## Notes

The tag archive carries its git submodules as empty directories. Of the three, SDL and fmt are
ports and come from the system; adpcm, superctr's ADPCM codec library, is compiled into Furnace
and has no port, so it is the recipe's second source, pinned to a commit (`_adpcm` in
`kpkgbuild`), and `build.sh` copies it into `extern/adpcm/` before the configure.

The recipe writes its own `furnace.desktop`, because upstream's `Exec=furnace` takes no file
argument; the replacement has `Exec=furnace %f`. Furnace sets SDL's Wayland window class to
`org.tildearrow.furnace`, which the entry names in `StartupWMClass`. `audio/x-fur` is the type
upstream's `res/mime.xml`, installed as `/usr/share/mime/packages/furnace.xml`, defines for
Furnace's own song files.
