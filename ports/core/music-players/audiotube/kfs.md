## About

AudioTube is a Kirigami client for YouTube Music: it searches the catalogue, streams songs,
albums and playlists, and keeps a local library of favourites and history. The C++ program embeds
Python through [python3-pybind11](#port-python3-pybind11) and calls
[python3-ytmusicapi](#port-python3-ytmusicapi) for the catalogue and [yt-dlp](#port-yt-dlp) for
the stream addresses; playback goes through Qt Multimedia.

## Flags

- `-D pybind11_DIR` — Where pybind11's CMake package is: the directory
  `python3 -m pybind11 --cmakedir` prints. [python3-pybind11](#port-python3-pybind11) is installed
  by pip as a Python module, and the module reports where its CMake files are, so
  `find_package(pybind11 REQUIRED)` finds the copy that module carries, as in the tree's other
  pybind11 consumers (`freecad`, `gnuradio`).
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

AudioTube plays only through Qt Multimedia's GStreamer backend: the configure stops with
`QtMultimedia gstreamer backend not found` unless the `Qt6::QGstreamerMediaPlugin` target exists.
[qt6-qtmultimedia](#port-qt6-qtmultimedia) installs it because it builds the GStreamer backend
beside the FFmpeg one; a qt6-qtmultimedia built with FFmpeg alone leaves this port unable to
configure. [gstreamer](#port-gstreamer), [gst-plugins-base](#port-gst-plugins-base),
[gst-plugins-good](#port-gst-plugins-good) and [gst-plugins-bad](#port-gst-plugins-bad) are in
`depends` for the decoders and demuxers the streams need at run time.

The configure imports `yt_dlp` and `ytmusicapi` with the build's Python and fails if either
import fails, or if ytmusicapi is older than 1.10.2. At run time the program compares ytmusicapi's
version with `TESTED_YTMUSICAPI_VERSION` in `src/ytmusic.h` and shows an "untested version" error
at every start for any other, so [python3-ytmusicapi](#port-python3-ytmusicapi) is held at exactly
that version. Both modules talk to an unofficial interface
of a commercial service, and a change on YouTube's side breaks them until they are bumped; the
program itself does not change.

[purpose](#port-purpose) provides the share menu and [qt6-qt5compat](#port-qt6-qt5compat) the
`Qt5Compat.GraphicalEffects` QML module. [qqc2-desktop-style](#port-qqc2-desktop-style) is in
`depends` because `main.cpp` selects its `org.kde.desktop` style when no other is set.
`KAboutData` gives the window the app_id `org.kde.audiotube`, the name of upstream's entry.
