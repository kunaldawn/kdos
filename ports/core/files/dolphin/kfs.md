## About

Dolphin is KDE's file manager: tabs, split views, the Places panel, previews, file metadata and
tags, and a terminal panel. It is built on KDE Frameworks 6 and Qt 6, as an application; nothing
of the desktop links it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6BalooWidgets=ON`: [baloo](#port-baloo) and
  [baloo-widgets](#port-baloo-widgets) are optional to upstream, and without both Dolphin drops
  the information panel, the metadata columns and the tag places with only a warning at
  configure time. Required, a missing one stops the configure.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as an optional component.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — No KUserFeedback, which Dolphin uses to
  submit telemetry. It is not a port; refusing it means a stray copy in the build root cannot be
  picked up.
- `-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON` — No PackageKit integration, which the service
  menu installer uses to install software. PackageKit is not a port either.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

- `app-id.patch` — Changes `StartupWMClass=dolphin` to `StartupWMClass=org.kde.dolphin` in upstream's desktop entry. Dolphin's Wayland app_id is its desktop file name, `org.kde.dolphin`, and upstream's shorter class matches no window Dolphin opens.

## Notes

musl has no `fts(3)`. The folder-size counter's configure check finds `fts.h` and `libfts` from
[musl-fts](#port-musl-fts), and links `-lfts` when the C library lacks `fts_open`.

[konsole](#port-konsole), whose part is the terminal panel, and [kio-extras](#port-kio-extras),
the thumbnailers, are in `depends` for run time only; the build does not check for them. Devices in
the Places panel come from Solid over [udisks2](#port-udisks2).

The install puts a systemd user unit, `plasma-dolphin.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)). `kdoctools_install()`
builds every translated manual page too, and `build.sh` removes every directory under
`/usr/share/man` that is not a `man<section>` one.

The desktop entry names the icon `org.kde.dolphin`. Upstream installs it as an SVG only, in
`hicolor/scalable`; the PNGs in its source become the macOS and Windows application icons. The
session reads no SVG icon (see
[Desktop entries belong to the port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so `build.sh` renders Breeze's `system-file-manager` SVG with `rsvg-convert` from
[librsvg](#port-librsvg) into PNGs named `org.kde.dolphin` in three hicolor sizes.
