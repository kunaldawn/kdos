## About

Kalm guides breathing exercises: box breathing, 4-7-8, resonant, coordinated, yogic and
alternate-nostril breathing, each with an animated shape to follow at the technique's pace. It is
a small Kirigami application from KDE's convergent set, on the `education` shelf as a teaching
tool.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON` — Make [kcrash](#port-kcrash) a requirement.
  Upstream asks for it as an optional component and builds without crash handling when it is
  missing.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`desktop-mime.patch` drops `MimeType=application/x-kalm;` from the desktop entry. Nothing
defines that type in the shared MIME database, so the claim matches no file (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).

## Notes

Upstream's top-level `CMakeLists.txt` adds two targets to `all`, `clang-format-always` and
`qml-format-always`, which run `clang-format -i` and `qmlformat -i` over the unpacked sources
while the rest of the build compiles them. Both tools are in the build root. `build.sh` builds the
two formatting targets, `clang-format` and `qml-format`, before the full build, so every file is
formatted once before anything compiles it; in the full build the `-always` targets find nothing
left to change. Built in parallel, a QML file could be compiled into the program either before or
after its reformat, and two builds of the same tree would differ.

[kirigami-addons](#port-kirigami-addons) and
[qqc2-desktop-style](#port-qqc2-desktop-style) are both `REQUIRED` by the configure, though
upstream marks the style `TYPE RUNTIME`: the program links nothing of it and selects the
`org.kde.desktop` Qt Quick style at start-up. [kirigami](#port-kirigami) is looked for only on
Android; on Linux the interface imports its `org.kde.kirigami` QML module at run time, which is
why it is in `depends`.
