## About

kglobalacceld is the registry of global shortcuts: which component owns which key combination,
the shortcuts every application registers through [kglobalaccel](#port-kglobalaccel), and the
actions desktop entries declare. It is a library, `libKGlobalAccelD`, not a program of its own:
[kwin](#port-kwin) links it, serves `org.kde.kglobalaccel` on the session bus with it, and
matches every key press against it before a window sees the key. The shortcuts are kept in
`~/.config/kglobalshortcutsrc`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D qdbus_EXECUTABLE=/usr/bin/qdbus6` — The command compiled into the shortcuts it migrates
  from an old `khotkeysrc`: a D-Bus action there becomes a command line that runs `qdbus` with the
  action's service, path and method. Upstream takes the first of `qdbus`, `qdbus6` and `qdbus-qt6`
  on `PATH` at configure time, and falls back to a bare `qdbus` when none is there. The tree's
  [qt6-qttools](#port-qt6-qttools) installs the Qt 6 client as `/usr/bin/qdbus6` (the plain name is
  only in `/usr/lib/qt6/bin`), so naming the path keeps a migrated shortcut working whatever the
  build chroot's `PATH` holds.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The install has no executable, no D-Bus activation file and no autostart entry: the shortcuts
work while KWin runs and not before, so the SDDM greeter, which runs KWin with
`--no-global-shortcuts`, has none. The library is installed without the unversioned
`libKGlobalAccelD.so` link a `-lKGlobalAccelD` would need; a consumer links it through the CMake
package `KGlobalAccelD`, whose target names the versioned file, as KWin does.
