## About

KScreen is the user side of display configuration in Plasma. Its Display Configuration page in
System Settings arranges the outputs and sets each one's mode, scale, rotation, refresh rate,
brightness, colour profile and HDR; [kwin](#port-kwin) applies the change, and
[libkscreen](#port-libkscreen) is the library the page talks to KWin through. The port also
installs the switch-display OSD that Meta+P opens, `kscreen_osd_service`; the Display
Configuration applet, with presentation mode and the layout picker, which the system tray lists
under Hardware; `kscreenctl`, which lists and sets outputs from a shell, with fish and zsh
completions; `kscreen-console`, which prints the configuration for debugging; and
`hdrcalibrator`, which the page and `kscreenctl` start to measure an HDR monitor's brightness
range.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WaylandClient=ON` — Makes Qt's Wayland client module from
  [qt6-qtwayland](#port-qt6-qtwayland), with its private part, a requirement of the configure.
  Upstream looks for it as optional, but `hdrcalibrator` speaks the `color-management-v1`
  protocol through it and links it unconditionally, so without it the build stops at that link
  rather than at the configure.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The OSD service is started by D-Bus activation and quits when it has been idle. The install writes
`/usr/share/dbus-1/services/org.kde.kscreen.osdService.service`, whose `Exec=` line names
`/usr/lib/libexec/kscreen_osd_service`; its `SystemdService=` line is read only by a bus that
systemd starts. The install also puts the systemd user unit that line names,
`plasma-kscreen-osd.service`, under `/usr/lib/systemd`; `build.sh` removes the directory, since
KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
The Meta+P shortcut is a desktop entry in `/usr/share/kglobalaccel` whose action runs `dbus-send`
from [dbus](#port-dbus) to wake the service.

The configure requires two QML modules to be installed: `org.kde.kitemmodels` from
[kitemmodels](#port-kitemmodels) for the page, and `org.kde.plasma.plasma5support` from
[plasma5support](#port-plasma5support) for the applet, whose presentation mode and list of what
is keeping the screen awake come from its `powermanagement` data engine. The page and the applet
also import `org.kde.kirigami` from [kirigami](#port-kirigami) at run time, and the calibrator's
test picture is an AVIF file, which Qt reads through the AVIF plugin of
[kimageformats](#port-kimageformats).
