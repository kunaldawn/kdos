## About
`kdos-transcribe` adds Transcribe to the context menu of Dolphin and every KDE file dialog for
audio and video files: `kf6/kfileitemaction/kdostranscribe.so`. Choosing it runs
`kdos speech transcribe --notify -- <files>`, which writes what is said into a `.txt` file beside
each one with [whisper.cpp](#port-whisper.cpp) and a notification when it is done. Nothing leaves
the machine.

The entry is there only while it can work: with `whisper-cli` on the `PATH`, `kdos` installed,
every selected item a local audio or video file, and a speech model where `kdos speech` looks for
one: `$KDOS_WHISPER_MODEL` when it names a file, then the first `ggml-*.bin` in
`~/.local/share/whisper.cpp/models`, then in each system data directory's `whisper.cpp/models`,
where [whisper-model-base-en](#port-whisper-model-base-en) puts its model.

## Flags
- `-S "$PORT_SRC"` — The sources are the port's own directory: a port of KDOS's own has no
  tarball, and the build tree is made in the empty `$SRC`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the plugin into Qt's own plugin directory,
  `/usr/lib/qt6/plugins/kf6/kfileitemaction`, where KIO looks for file-item actions.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. The QtTest suite in `tests/` is run
  by the Qt regression net in the development container, not by the image build.
- `-D CMAKE_INSTALL_PREFIX=/usr`, `-D KDE_INSTALL_LIBDIR=lib`, `-D CMAKE_BUILD_TYPE=Release` —
  The tree's prefix and library directory, which ECM would otherwise put under `lib64`, and an
  optimised build.

## Notes
Transcribe is a file-item action plugin rather than a service menu. A service menu is a desktop
file KIO lists whenever it is installed, and KIO 6 has no condition that hides one while a
program or a file is missing; the plugin decides each time the menu opens, so the entry is gone
on an image with no model. Its metadata names `application/octet-stream`, which every audio and
video type inherits, and the plugin itself narrows that to `audio/` and `video/`.

The file names go to `kdos` as arguments of their own after `--`, so a name that starts with `-`
is never read as an option.
