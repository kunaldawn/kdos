## About

LMMS is a pattern-based music production program: a song editor, beat and bassline patterns, a
piano roll and a mixer, with built-in synthesizers (among them ZynAddSubFX, an emulated NES, SID
and OPL2), a sampler, and LADSPA effects. It is built against Qt 5. The bundled ZynAddSubFX
editor is drawn with [FLTK](#port-fltk).

## Flags

- `-DCMAKE_CXX_FLAGS` — the tree's `CXXFLAGS`, `-Wno-deprecated-declarations`, which silences
  the deprecation warnings, and `$(pkg-config --cflags cairo)`. The fltk port is built with Cairo,
  so `FL/Fl_Cairo.H` includes `<cairo.h>`, which sits in `/usr/include/cairo`; LMMS finds FLTK
  through CMake's `FindFLTK`, which gives the ZynAddSubFX editor FLTK's include directory alone
  (see
  [FLTK's headers needing Cairo's](../../../../docs/kdos/05-developer/build-troubleshooting.md#fltks-headers-needing-cairos)).
  That is also why [cairo](#port-cairo) is in `depends`.
- `-DFORCE_VERSION=$version` — the version LMMS reports. Without it the version is read from
  `git describe` when git is installed, and a source tarball is not a git checkout.
- `-DBASHCOMP_PKG_PATH="$PKG/usr/share/bash-completion/completions"` — where the bash completion
  script goes. LMMS's install script writes to this directory directly, without `DESTDIR`, whenever
  it is writable, so a plain `/usr/share/bash-completion/completions` would put the file in the
  build root instead of the package. `build.sh` checks that it landed in `$PKG`.
- `-DWANT_QT5=ON` — build against Qt 5; without it, upstream builds against Qt 4.
- `-DWANT_ALSA=ON` — and `WANT_PULSEAUDIO`, `WANT_PORTAUDIO`, `WANT_SOUNDIO`, `WANT_SDL`,
  `WANT_JACK`: the audio back ends, through ALSA, PulseAudio, [portaudio](#port-portaudio),
  [libsoundio](#port-libsoundio), SDL 1.2 ([sdl12-compat](#port-sdl12-compat)) and JACK, all of
  which PipeWire serves on KDOS.
- `-DWANT_WEAKJACK=OFF` — link JACK directly, against the `libjack` the [pipewire](#port-pipewire)
  port provides, instead of loading its functions with `dlsym` at run time (upstream's default).
- `-DWANT_CARLA=OFF` — no Carla plugin host; Carla is not a port.
- `-DWANT_VST=OFF` — no VST plugin hosting, which LMMS builds against Wine.
- `-DWANT_SNDIO=OFF` — no sndio back end; sndio is not a port.
- `-DWANT_GIG=ON` — and `WANT_STK`, `WANT_SF2`: the GIG sample player on [libgig](#port-libgig),
  the Mallets instrument on [stk](#port-stk), which loads its samples from STK's
  `/usr/share/stk/rawwaves/`, and the SoundFont player on [fluidsynth](#port-fluidsynth).
- `-DWANT_MP3LAME=ON` — and `WANT_OGGVORBIS`: MP3 export through [lame](#port-lame) and Ogg Vorbis
  export.
- `-DWANT_CALF=ON` — and `WANT_CAPS`, `WANT_CMT`, `WANT_SWH`, `WANT_TAP`: the LADSPA plugin suites
  LMMS carries in its own tree under `plugins/LadspaEffect/`.

## Patches

- `fix-cmake-policy.patch` — In the top-level `CMakeLists.txt`, sets policies `CMP0026` and `CMP0050` to `NEW` instead of `OLD`, which CMake 4 refuses (see [a policy set to its removed behaviour](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-policy-set-to-its-removed-behaviour)), and replaces the one read of a target's `LOCATION` property (`GET_TARGET_PROPERTY(BIN2RES bin2res LOCATION)`, which `CMP0026` `NEW` forbids) with the generator expression `$<TARGET_FILE:bin2res>`.

## Notes

Each `WANT_*` switch asks for its library and, when the library is not found, drops the feature
with a status line only. So after the configure, `build.sh` checks that `build/lmmsconfig.h`
defines `LMMS_HAVE_ALSA`, `LMMS_HAVE_PULSEAUDIO`, `LMMS_HAVE_PORTAUDIO`, `LMMS_HAVE_JACK`,
`LMMS_HAVE_SOUNDIO`, `LMMS_HAVE_SDL` and `LMMS_HAVE_STK`, and after the install that the Mallets
and GIG player plugins were installed, so a missing library fails the build. Qt takes the Wayland
app id from the program name, `lmms`, which the recipe's desktop entry repeats in
`StartupWMClass`.
