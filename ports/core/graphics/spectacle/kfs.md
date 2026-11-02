## About

Spectacle is Plasma's screenshot and screen-recording tool: the whole desktop, one monitor, a
window or a rectangular region, annotated in place with [kquickimageeditor](#port-kquickimageeditor),
with text recognition over [tesseract](#port-tesseract), and video of a region, a screen or a
window. Under KWin a still comes through KWin's `org.kde.KWin.ScreenShot2` D-Bus interface and a
recording through the `zkde_screencast_unstable_v1` protocol and a PipeWire stream, which
[kpipewire](#port-kpipewire) encodes to a file. It is the desktop's capture tool: the Print key
and its chords start it.

## Flags

- `-D WITH_X11=OFF` — Build no XCB capture platform. Spectacle uses it only in an X11 session,
  which KDOS does not have; under KWin's Wayland session the KWin platform is chosen whatever this
  says, so the switch drops five XCB utility libraries from the build and nothing from what the
  program does.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The manual page `spectacle(1)` and the
  handbook, through [kdoctools](#port-kdoctools), which upstream looks for as optional and skips
  without a word.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON` — The Share menu, through
  [purpose](#port-purpose). Upstream compiles it in only when the framework is found.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The install puts a systemd user unit, `app-org.kde.spectacle.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)). D-Bus activation
does not need it: both service files, `org.kde.spectacle.service` and `org.kde.Spectacle.service`,
carry `Exec=/usr/bin/spectacle --dbus` beside their `SystemdService=` line, and a bus daemon
without systemd activation runs the `Exec=` command.

The install links `/usr/share/kglobalaccel/org.kde.spectacle.desktop` to the application entry,
and the global shortcut daemon reads the entry's `X-KDE-Shortcuts` keys from it: Print and
Meta+Shift+S open Spectacle, Shift+Print takes the whole desktop, Meta+Print the active window,
Meta+Shift+Print a region and Meta+Ctrl+Print the window under the pointer; Meta+Shift+R or Meta+R,
Meta+Alt+R and Meta+Ctrl+R start and stop region, screen and window recordings. The distribution
defaults override part of that set: `kglobalshortcutsrc` under `/usr/share/kdos/xdg` leaves
Print alone on the launch action, takes the whole-desktop shot off Shift+Print, puts a rectangle
on Shift+Print, Meta+Shift+Print and Meta+Shift+S and adds Alt+Print to the region recording, and
`spectaclerc` beside it makes the whole screen the launch action, so Print takes the whole screen.

KWin answers `ScreenShot2` only for a program whose desktop entry names that interface in
`X-KDE-DBUS-Restricted-Interfaces` and whose `Exec=` resolves to the caller's executable. The
entry names `/usr/bin/spectacle`, so a Spectacle run from any other path is refused every capture.

Spectacle reads a QR code or barcode in a capture with [prison](#port-prison)'s `PrisonScanner`
library, and the configure stops with "Prison built without required PrisonScanner component"
when prison was built without it; the prison port builds it over
[qt6-qtmultimedia](#port-qt6-qtmultimedia) and [zxing-cpp](#port-zxing-cpp).

Text recognition needs tesseract's pkg-config module at configure time, with no switch to leave it
out. The language list offers the trained data installed under `/usr/share/tessdata`;
[tessdata-eng](#port-tessdata-eng), which tesseract depends on, gives English.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
translated manual pages under `/usr/share/man/<lang>/` and the translated handbooks, and kpkg's
language rule removes both from the package.
