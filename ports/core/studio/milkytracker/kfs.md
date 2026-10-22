## About

MilkyTracker is a music tracker in the style of Fast Tracker II: it composes and plays XM and MOD
modules, with Fast Tracker II's screen layout and keys. It draws its interface itself on SDL2
([sdl2-compat](#port-sdl2-compat)), plays through ALSA (and so PipeWire) and SDL, and takes MIDI
input through [rtmidi](#port-rtmidi).

## Flags

- `-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON` — never look for JACK. On Linux the CMakeLists calls
  `find_package(JACK)` and builds the JACK output whenever it is found; the
  [pipewire](#port-pipewire) port installs a `libjack`, so without this switch the result would
  depend on what the build root holds.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_LHASA=ON` — and `-DCMAKE_REQUIRE_FIND_PACKAGE_ZZIPLIB=ON`:
  MilkyTracker opens modules packed in LHA and ZIP archives through [lhasa](#port-lhasa) and
  [zziplib](#port-zziplib), and its CMakeLists looks for both as optional. Required, a missing one
  stops the configure instead of building a tracker that cannot open those archives (see
  [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Patches

- `milkytracker-cmake4.patch` — Deletes `cmake_policy(SET CMP0004 OLD)` from the top-level `CMakeLists.txt`, a workaround for a bug in SDL2 releases older than 2.0.5. CMake 4 refuses `OLD` for that policy and stops the configure (see [a policy set to its removed behaviour](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-policy-set-to-its-removed-behaviour)), and the tree's SDL2, sdl2-compat, is a later release than the bug affects.

## Notes

MIDI support is built only when the configure finds ALSA and RtMidi, and it is not required: a
missing rtmidi prints `MIDI support disabled (RtMidi unavailable)` and builds a tracker with no
MIDI input.

Upstream installs the program, its documents and example songs, but no icon or desktop entry.
The recipe installs `resources/pictures/carton.png`,
upstream's 128-pixel icon, as the `milkytracker` icon, and writes its own desktop entry. SDL names
the window after the program, `milkytracker`, which the entry repeats in `StartupWMClass`;
upstream's `resources/milkytracker.desktop` claims the MOD and XM types, and the recipe's claims
none.
