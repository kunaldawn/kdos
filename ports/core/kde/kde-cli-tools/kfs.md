## About

KDE CLI Tools are the small programs the desktop and its scripts call by name: `kde-open`,
`kioclient`, `kdecp` and `kdemv` (open, copy and move any KIO URL), `kstart` (start a program with
a window rule), `keditfiletype` and the File Types settings module (the editor Dolphin's "Edit File
Type" opens), `kmimetypefinder`, `kinfo`, `ksvgtopng`, `kde-inhibit`, `kbroadcastnotification`,
`plasma-open-settings`, which opens a System Settings page from a `systemsettings://` link, and, in
`/usr/lib/libexec/kf6`, `kdesu` (run a graphical program as another user) and `kdeeject`. Eight
of the programs in `/usr/bin` also answer to a name with a `5` suffix (`kioclient5`, `kde-open5`,
`kstart5` and the rest), the names that scripts and desktop entries written for Plasma 5 call.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Su=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6WidgetsAddons=ON`: `kdesu` is built only when both
  [kdesu](#port-kdesu) and [kwidgetsaddons](#port-kwidgetsaddons) are found, and upstream asks for
  them as optional components and says nothing when they are missing. Without `kdesu` a desktop
  entry or service menu that runs a program as root has nothing to run it with.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`kdesu` hands the command to the KDESu framework, which the [kdesu](#port-kdesu) port builds to use
[sudo](#port-sudo): the dialog asks for the caller's own password, and `/etc/sudoers` decides
whether the command may run.

`WITH_X11` is left at its default, on. It compiles in the X11 paths of `kdesu` and `kstart`, which
reach an Xwayland client's window, and links nothing but Qt's private GUI module.

The handbooks are built by default and make [kdoctools](#port-kdoctools) a requirement of the
configure.
