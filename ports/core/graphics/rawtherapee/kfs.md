## About

RawTherapee is a raw photo developer: it turns a camera's raw file into a finished image through
a long chain of demosaicing, exposure, colour, sharpening and lens-correction tools, and keeps the
settings for each image in a `.pp3` processing profile beside it rather than changing the file.
The port builds its GTK 3 interface, `rawtherapee`, and `rawtherapee-cli`, which applies
profiles to files from a shell.

## Flags

- `-D WITH_SYSTEM_FMT=ON` — Uses the tree's [fmt](#port-fmt). Left at its default,
  `cmake/Dependencies.cmake` clones fmt from GitHub with `FetchContent` at configure time, which
  the build has no network for.
- `-D WITH_SYSTEM_LIBRAW=ON` — Uses the tree's [libraw](#port-libraw) instead of building the copy
  in the tarball.
- `-D WITH_SYSTEM_KLT=OFF` — Builds the KLT feature tracker in `rtengine/klt`, which is not a
  port; upstream's default, named.
- `-D PROC_TARGET_NUMBER=0` — Adds no `-march` or `-mtune` of its own, so the package runs on
  every x86-64 processor; upstream's default, named.
- `-D CACHE_NAME_SUFFIX=""` — The settings and cache live in `~/.config/RawTherapee` and
  `~/.cache/RawTherapee`, with no version suffix; upstream's default, named.
- `-D BUILD_BUNDLE=OFF` — An ordinary install under the prefix, with absolute data paths, rather
  than a relocatable bundle.
- `-D BUILD_SHARED=OFF` — Upstream's default, named. The option is declared in the top-level
  `CMakeLists.txt`, and nothing else in the Linux build reads it.
- `-D OPTION_OMP=ON` — Threads through OpenMP; upstream's default, named.
- `-D WITH_LTO=OFF`, `-D WITH_SIMDE=OFF`, `-D WARNINGS_AS_ERRORS=OFF`, `-D ENABLE_TCMALLOC=OFF` —
  Upstream's defaults, named: no link-time optimisation, no SIMD Everywhere portability layer,
  warnings left as warnings, and the C library's allocator rather than tcmalloc.
- `-D WITH_JXL=ON` — JPEG XL through [libjxl](#port-libjxl). Upstream's `AUTO` would quietly
  leave it out when the library is missing; named `ON`, configure stops instead.
- `-D SVG_BACKEND=librsvg` — Draws the interface's SVG icons through
  [librsvg](#port-librsvg) rather than the alternative, lunasvg; upstream's default, named.
- `-D USE_LIBCANBERRA=ON` — Plays a sound through [libcanberra](#port-libcanberra) when the
  batch queue finishes; upstream's default on Linux, named.

## Notes

RawTherapee keeps its interface languages as plain text files in
`/usr/share/rawtherapee/languages`, which `kpkg`'s language rule does not match. The `find` after
the install removes all of them except `default`, the built-in English, the two English variants
beside it, and the directory's `LICENSE` and `README`.

Upstream's desktop entry claims `inode/directory`, so a folder could open in RawTherapee instead
of the file manager. It is replaced by one without that type, keeping the image and raw types
and `StartupWMClass=rawtherapee`, the Wayland app_id GTK 3 takes from the program's name. The
hicolor PNG icons it names are upstream's.
