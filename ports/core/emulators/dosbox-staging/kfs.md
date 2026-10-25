## About

DOSBox Staging emulates an IBM PC compatible running DOS, with its sound cards and graphics
adapters, to play DOS games and run old DOS software. It is an SDL 2 program, drawn here by
[sdl2-compat](#port-sdl2-compat) on SDL 3. MT-32 music comes from
[libmt32emu](#port-libmt32emu) and General MIDI from [fluidsynth](#port-fluidsynth); both need
data the player supplies, the Roland ROMs and a SoundFont.

## Flags

- `-DCMAKE_SKIP_RPATH=ON` — Write no run path. Upstream sets `$ORIGIN/lib`, a `lib/` directory
  beside the binary for its bundled vcpkg libraries, which a system install does not have.
- `-DHAVE_PTHREAD_SETNAME_NP=1` — Answer configure's check for `pthread_setname_np` in advance.
  musl declares it in `pthread.h` only under `_GNU_SOURCE`, which the check does not define, so the
  check fails although the function is there, and the emulator's threads would go unnamed.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON` — Do not look for git. With git found, configure runs
  `git rev-parse` in the source directory, which is no checkout; without it the build hash reads
  `?`.
- `-DIS_PRESET_USED=TRUE` — Upstream builds only through its CMake presets and warns when none is
  used; every preset sets this variable, and setting it here silences the warning.
- `-DUSE_SYSTEM_LIBS=ON` — Find opusfile through pkg-config, as upstream's Linux presets do,
  instead of as the `OpusFile` CMake package vcpkg writes. It is the variable's only effect; every
  other library is found the same way either way.
- `-DOPT_TESTS=OFF` — No GTest suite, which upstream builds by default.
- `-DOPT_DOCUMENTATION=OFF` — No offline MkDocs manual. It is upstream's default.
- `-DOPT_DEBUGGER=OFF` — No built-in DOS debugger. It is upstream's default.
- `-DOPT_OPENGL=ON` — The OpenGL renderer, upstream's default.
- `-DOPT_MT32EMU=ON` — Roland MT-32 emulation through libmt32emu, upstream's default.
- `-DOPT_MANYMOUSE=ON` — ManyMouse, which reads several mice separately for the two-player modes of
  The Settlers I and II. It is upstream's default.
- `-DOPT_XINPUT=OFF` — ManyMouse reads the mice through evdev, not through the X Input protocol.
  It is upstream's default.

## Notes

[libslirp](#port-libslirp), for the emulated network card, is loaded at run time.

`build.sh` removes `/usr/share/dosbox-staging/resources/translations`; the interface falls back to
its built-in English messages. Upstream's menu entry is replaced. The Wayland app_id is
`org.dosbox_staging.dosbox_staging`, set through SDL's app-id hint, and the icon of that name is
upstream's hicolor PNG set.
