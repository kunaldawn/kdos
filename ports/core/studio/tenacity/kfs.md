## About

Tenacity is a multitrack audio editor and recorder on wxWidgets, a fork of the Audacity 3 code
base with Audacity's network features removed. KDOS carries it beside [Audacity](#port-audacity),
which is the Qt 6 Audacity 4. Its formats and effects come from libraries in `depends`: libsndfile
for uncompressed formats, libmad for MP3 import and libid3tag for MP3 tags, LAME for MP3 export,
twolame for MP2 export, Ogg, Vorbis, FLAC and Matroska, SoundTouch and SBSMS for pitch and tempo,
and the LV2 and Vamp plugin hosts. The LADSPA and VST2 hosts are compiled from Tenacity's own
tree and need no library.

## Flags

- `-D CMAKE_SKIP_BUILD_RPATH=ON` — link the build tree's binaries with no `RPATH`. Tenacity
  copies its own libraries into place with `install(DIRECTORY)`, which ships them with the
  `RPATH` they were built with, a build directory the package lint refuses (see
  [what the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-D VCPKG=OFF` — take every dependency from the system instead of building it with vcpkg.
  Upstream's default is `ON` only on Windows and macOS; the switch states the Linux default.
- `-D SCCACHE=OFF` — and `-D CCACHE=OFF`: when sccache or ccache is on the `PATH`, upstream puts it
  in front of the compiler; off, the tree's compiler is called directly.
- `-D BUILD_STATIC_LIBS=OFF` — Tenacity's internal libraries are shared libraries, upstream's
  default.
- `-D ID3TAG=ON` — and `MP3_DECODING`, `OGG`, `VORBIS`, `FLAC`, `SOUNDTOUCH`, `VAMP`, `LV2`,
  `MIDI`, `MATROSKA`, `MP2`: each feature by name. Every one of these is a
  `cmake_dependent_option` that turns itself off when its library is not found and says so only
  in a status line, so naming it `ON` documents the intent but does not make a missing library
  fail the build; the libraries are all in `depends`. MIDI needs [portmidi](#port-portmidi) and
  [portsmf](#port-portsmf), Matroska [libmatroska](#port-libmatroska) over
  [libebml](#port-libebml), and MP2 export [twolame](#port-twolame).
- `-D SBSMS=ON` — SBSMS time-stretching from the [libsbsms](#port-libsbsms) port. Without vcpkg its
  default is `OFF`, so this switch is what turns it on.
- `-D LADSPA=ON` — and `-D VST2=ON`: hosting of LADSPA and VST2 effect plugins. VST2 hosting loads
  plugin shared objects the user installs; none ships with it.
- `-D FFMPEG=OFF` — no FFmpeg import and export. Tenacity loads FFmpeg's libraries at run time and
  can use only the libavformat major versions it has an implementation for, one directory each
  under `libraries/lib-ffmpeg-support/impl/avformat/`. The [ffmpeg](#port-ffmpeg) port's is newer
  than all of them, so the loader would never find a library it can use.
- `-D PERFORM_CODESIGN=OFF` — no code signing step, which applies to Windows and macOS builds.

## Notes

Upstream installs its PNG icons into `hicolor/<size>/` with no `apps/` level, where no icon lookup
finds them; `build.sh` moves each into `hicolor/<size>/apps/`.

The recipe writes its own `tenacity.desktop`. The Wayland app id is GLib's program name,
`tenacity`, which the entry names in `StartupWMClass`, and `Exec` runs `tenacity` itself rather than
upstream's `env UBUNTU_MENUPROXY=0` wrapper, a setting for Ubuntu's Unity menu proxy. The
entry has no `MimeType` line, so Tenacity is offered as the opener of no file type.
