## About

SDL3_mixer plays sound effects and music for SDL 3 programs, mixing any number of tracks into one
[sdl3](#port-sdl3) audio stream: WAV, AIFF, AU, VOC, FLAC, Ogg Vorbis, Opus, MP3 and WavPack files,
tracker modules, MIDI scores and the console chiptune formats. As in
[sdl2-mixer](#port-sdl2-mixer), its SDL 2 counterpart, every compressed format is decoded by the
library port that owns it, linked rather than loaded at run time, and the single-file decoders
upstream compiles in (dr_flac, dr_mp3, stb_vorbis) are left out.

## Flags

- `-DSDLMIXER_VENDORED=OFF` — Builds against the root's codec libraries rather than upstream's own
  copies, which are git submodules that `external/download.sh` fetches and the release tarball does
  not contain. This is upstream's default everywhere but MSVC and Android, named here.
- `-DSDLMIXER_STRICT=ON` — Stops configure when a library a named format needs is missing.
  Upstream's default is off, and a missing library then builds the library without that format.
- `-DSDLMIXER_DEPS_SHARED=OFF` — Links each codec library. Upstream's default loads each one with
  `dlopen` at run time instead; a dlopened library is a dependency the package solver cannot see,
  and a missing one is a format that silently stops playing.
- `-DSDLMIXER_TESTS=OFF`, `-DSDLMIXER_EXAMPLES=OFF` — Builds neither the unit tests nor the example
  programs, which upstream builds by default when SDL3_mixer is the top-level project.
- `-DSDLMIXER_INSTALL_MAN=ON` — Generates and installs a manual page for every public function.
  Upstream's default is off. The pages are written from the headers by
  `build-scripts/wikiheaders.pl`, which is why [perl](#port-perl) is in `depends`.
- `-DSDLMIXER_FLAC_LIBFLAC=ON`, `-DSDLMIXER_FLAC_DRFLAC=OFF` — FLAC through [flac](#port-flac)'s
  libFLAC alone; upstream also builds the dr_flac copy by default.
- `-DSDLMIXER_MP3_MPG123=ON`, `-DSDLMIXER_MP3_DRMP3=OFF` — MP3 through [mpg123](#port-mpg123)
  alone; upstream also builds the dr_mp3 copy by default.
- `-DSDLMIXER_VORBIS_VORBISFILE=ON`, `-DSDLMIXER_VORBIS_STB=OFF`, `-DSDLMIXER_VORBIS_TREMOR=OFF` —
  Ogg Vorbis through [libvorbis](#port-libvorbis)'s vorbisfile alone, without the stb_vorbis copy
  upstream builds by default and without Tremor, the integer-only decoder, which is not a port.
- `-DSDLMIXER_MOD_XMP=ON`, `-DSDLMIXER_MOD_XMP_LITE=OFF` — Tracker modules through the full
  [libxmp](#port-libxmp), not libxmp-lite, which plays fewer formats.
- `-DSDLMIXER_MIDI_FLUIDSYNTH=ON` — MIDI through [fluidsynth](#port-fluidsynth), tried before the
  built-in Timidity. With no SoundFont given by the program, it opens the first of three
  distribution paths that exists; `/usr/share/soundfonts/default.sf2`, which
  [fluidr3-gm-sf3](#port-fluidr3-gm-sf3) installs, is one of them. With none of the three present,
  FluidSynth still accepts the score and plays it silent; it does not pass it on to Timidity.
- `-DSDLMIXER_MIDI_TIMIDITY=ON` — SDL3_mixer's own copy of Timidity, which plays a score only when
  FluidSynth refuses it. It needs a `timidity.cfg` and a patch set, named by `TIMIDITY_CFG` or found
  at one of three paths under `/etc` or as `timidity.cfg` in the working directory, and no port
  installs one.
- `-DSDLMIXER_FLAC=ON`, `-DSDLMIXER_GME=ON`, `-DSDLMIXER_MOD=ON`, `-DSDLMIXER_MP3=ON`,
  `-DSDLMIXER_MIDI=ON`, `-DSDLMIXER_OPUS=ON`, `-DSDLMIXER_WAVPACK=ON` — Upstream's defaults, named
  in the recipe. The chiptune formats go through [libgme](#port-libgme), Opus through
  [opusfile](#port-opusfile), WavPack through [wavpack](#port-wavpack).
