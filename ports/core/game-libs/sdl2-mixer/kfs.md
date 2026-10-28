## About

SDL2_mixer plays sound effects and music for SDL 2 programs: WAV, FLAC, Ogg Vorbis, Opus, MP3 and
WavPack files, tracker modules, MIDI scores and the console chiptune formats. It is built against
[sdl2-compat](#port-sdl2-compat), so its sound goes out through [sdl3](#port-sdl3). Every compressed
format is decoded by the library port that owns it, linked rather than loaded at run time, and the
decoders upstream compiles in as single-file copies (dr_flac, minimp3, stb_vorbis) are left out.

## Flags

- `-DSDL2MIXER_VENDORED=OFF` — Builds against the root's codec libraries rather than upstream's own
  copies, which are git submodules that `external/download.sh` fetches and the release tarball does
  not contain. This is upstream's default everywhere but MSVC and Android, named here.
- `-DSDL2MIXER_DEPS_SHARED=OFF` — Links each codec library. Upstream's default loads each one with
  `dlopen` at run time instead; a dlopened library is a dependency the package solver cannot see,
  and a missing one is a format that silently stops playing.
- `-DSDL2MIXER_CMD=OFF` — Builds without the external-player interface, through which a program
  that calls `Mix_SetMusicCMD` hands its music files to an outside command that SDL2_mixer forks and
  runs. `Mix_SetMusicCMD` still returns success, the command is never run, and the file is decoded
  in the library. Upstream's default on Linux is on.
- `-DSDL2MIXER_FLAC_LIBFLAC=ON`, `-DSDL2MIXER_FLAC_DRFLAC=OFF` — FLAC through [flac](#port-flac)'s
  libFLAC instead of the dr_flac copy compiled into the library, which is upstream's default.
  SDL2_mixer tries dr_flac first when both are built.
- `-DSDL2MIXER_GME=ON` — The console and computer chiptune formats (NSF, SPC, GBS, VGM and the
  rest) through [libgme](#port-libgme). Upstream's default is off.
- `-DSDL2MIXER_MOD_XMP=ON`, `-DSDL2MIXER_MOD_XMP_LITE=OFF`, `-DSDL2MIXER_MOD_MODPLUG=OFF` — Tracker
  modules through the full [libxmp](#port-libxmp), not libxmp-lite, which plays fewer formats.
  ModPlug stays off although [libmodplug](#port-libmodplug) is a port: it and libxmp claim the same
  formats, and the first decoder that accepts a file plays it.
- `-DSDL2MIXER_MP3_MPG123=ON`, `-DSDL2MIXER_MP3_MINIMP3=OFF` — MP3 through [mpg123](#port-mpg123)
  instead of the minimp3 copy compiled into the library, which is upstream's default and is tried
  first when both are built.
- `-DSDL2MIXER_MIDI_FLUIDSYNTH=ON` — MIDI through [fluidsynth](#port-fluidsynth), tried before the
  built-in Timidity. With no SoundFont set by the program or in `SDL_SOUNDFONTS`, the one file it
  looks for is `/usr/share/sounds/sf2/FluidR3_GM.sf2`, which [fluidr3-gm-sf3](#port-fluidr3-gm-sf3)
  installs. Without a SoundFont the FluidSynth player does not open, and a MIDI score falls to
  Timidity.
- `-DSDL2MIXER_MIDI_TIMIDITY=ON` — SDL2_mixer's own copy of Timidity, the fallback when FluidSynth
  cannot play. It needs a `timidity.cfg` and a patch set, named by `TIMIDITY_CFG` or by the program,
  or found at `/etc/timidity.cfg`, at `/etc/timidity/freepats.cfg` or as `timidity.cfg` in the
  working directory, and no port installs one, so with neither player able to open a MIDI score
  does not load.
- `-DSDL2MIXER_VORBIS=VORBISFILE` — Ogg Vorbis through [libvorbis](#port-libvorbis)'s vorbisfile
  instead of `STB`, the stb_vorbis copy compiled into the library, which is upstream's default.
- `-DSDL2MIXER_SAMPLES=OFF` — Builds none of the sample players, which upstream builds by default
  when SDL2_mixer is the top-level project.
- `-DSDL2MIXER_FLAC=ON`, `-DSDL2MIXER_MOD=ON`, `-DSDL2MIXER_MP3=ON`, `-DSDL2MIXER_MIDI=ON`,
  `-DSDL2MIXER_OPUS=ON`, `-DSDL2MIXER_WAVE=ON`, `-DSDL2MIXER_WAVPACK=ON` — Upstream's defaults,
  named in the recipe. Opus goes through [opusfile](#port-opusfile), WavPack through
  [wavpack](#port-wavpack).

## Notes

[sdl3-mixer](#port-sdl3-mixer) is the SDL 3 counterpart, built with the same choice for every
codec.
