## About

KCoreAddons is the non-GUI core of KDE Frameworks: the plugin loader and its JSON metadata
(`KPluginMetaData`, `KPluginFactory`), `KJob` for asynchronous work, `KAboutData`, directory
watching (`KDirWatch`, over inotify), filesystem type detection, process and user information, and
string and text helpers. Almost every KDE library and application in the tree links it.

## Flags

- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_UDev=ON` — Make libudev a requirement. Upstream looks for it
  quietly on Linux and uses it in `KFileSystemType` to tell the real filesystem under a FUSE block
  mount from a FUSE filesystem with no device behind it; without
  libudev both are reported as plain FUSE. The library comes from [eudev](#port-eudev), which is in
  `depends`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch also skips upstream's `en`
  catalogue, which holds only English plural forms, so a count `KFormat` writes out reads as the
  source spells it, `5 millisecond(s)` rather than `5 milliseconds`.

## Notes

`KFileSystemType` also asks libmount which filesystem a path is on, and `find_package(LibMount
REQUIRED)` on Linux makes [util-linux](#port-util-linux) a requirement of the configure with no
switch needed.

The QML module `org.kde.coreaddons` (`KCOREADDONS_USE_QML`, on by default) requires Qt Qml, which
is why [qt6-qtdeclarative](#port-qt6-qtdeclarative) is in `depends`.
