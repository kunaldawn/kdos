## About

KRecorder is the Plasma Mobile voice recorder: it records from the microphone, lists the clips
it has made, and plays, renames and deletes them. It is a small Kirigami program that works the
same way on a desktop.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Recording and playback go through Qt Multimedia, which [qt6-qtmultimedia](#port-qt6-qtmultimedia)
builds with its FFmpeg backend; the input device is whatever capture source the session's audio
server offers, so a machine with no capture device shows an empty input list.

The configure looks for `KF6Kirigami2` as a run-time package only and continues without it;
[kirigami](#port-kirigami) is in `depends` because the interface is Kirigami QML, which CMake
does not check. [qqc2-desktop-style](#port-qqc2-desktop-style) is in `depends` because `main.cpp`
selects its `org.kde.desktop` style when no other is set. `KAboutData` gives the window the
app_id `org.kde.krecorder`, the name of upstream's entry.
