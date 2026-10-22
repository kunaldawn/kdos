## About

libdvdread reads the UDF file system and the title, chapter and cell structure of a DVD-Video
disc. [libdvdnav](#port-libdvdnav) is built on it for menu navigation, and ffmpeg, VLC, mpv,
GStreamer's `dvdreadsrc`, k3b and MKVToolNix read discs through one or both. It decrypts a CSS-scrambled disc, which most
commercial discs are, through [libdvdcss](#port-libdvdcss); on its own it opens only unscrambled
ones.

## Flags

- `-Ddefault_library=shared` — Build only the shared library.
- `-Dlibdvdcss=enabled` — Link libdvdcss directly, and fail the configure if it is missing. At
  `auto`, upstream's default, a build that does not find libdvdcss loads `libdvdcss.so.2` with
  `dlopen` at run time instead; when that fails it logs only a warning, `Encrypted DVD support
  unavailable.`, and reads a scrambled disc without decrypting it.
- `-Denable_docs=false` — No Doxygen API reference. It is upstream's default.

## Patches

- `musl-off64_t.patch` — The installed `dvd_filesystem.h`, reached through `dvd_reader.h`, uses `off64_t` and declares it itself only for MSVC, glibc, macOS and the BSDs. On musl, `sys/types.h` names `off64_t` only when it was first included with `_LARGEFILE64_SOURCE` defined, so a consumer that included `sys/types.h` first fails to compile (libdvdnav, mpv and gst-plugins-bad among them). The patch adds `typedef off_t off64_t;` for every other system that has not defined it, which is exact on musl, whose `off_t` is always 64 bits.
