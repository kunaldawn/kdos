## About

audiocd-kio is the `audiocd:/` KIO worker. An audio CD opened in Dolphin shows its tracks as WAV
files, with folders that hold the same tracks as FLAC, Ogg Vorbis, Opus and MP3; copying a file out
of one rips and encodes that track. Track names come from CDDB through
[libkcddb](#port-libkcddb). A System Settings page sets the encoders' quality and the ripper's
error correction, and a Solid action offers to open a disc when one is inserted.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_FLAC=ON` — With `-D CMAKE_REQUIRE_FIND_PACKAGE_OggVorbis=ON`:
  make [flac](#port-flac) and [libvorbis](#port-libvorbis) with [libogg](#port-libogg)
  requirements. The two encoders link them, and upstream leaves each encoder out without a word
  when its library is missing. The Ogg encoder also needs `vorbis_bitrate_addblock`, which the
  module checks for and every current libvorbis has.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Make [kdoctools](#port-kdoctools) a
  requirement. Upstream finds it as optional and drops the worker's and the settings page's
  handbooks without one.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

Reading the disc is xiph's cdparanoia-III library from [cdparanoia](#port-cdparanoia), found by
the bundled `FindCdparanoia.cmake` as `cdda_interface.h`, `libcdda_interface` and
`libcdda_paranoia`; libcdio's paranoia, which installs other headers and names, does not satisfy
it. The configure copies `cdda_interface.h` into the build directory through `sed`, renaming a
member called `private` so the header compiles as C++; that is upstream's own build step and leaves
the installed header alone.

The MP3 and Opus encoders are not linked: they run [lame](#port-lame) and `opusenc` from
[opus-tools](#port-opus-tools) and pipe the audio to them, so both are in `depends` for run time,
and each encoder's folder is hidden when its program is not found. The WAV and CDA folders need
neither.

The drive is `/dev/sr0` or another `/dev/sr*` node, and only a user allowed to open it can read the
disc.
