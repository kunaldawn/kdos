## About

NetworkManagerQt is the Qt wrapper around NetworkManager's D-Bus API: devices, active connections,
saved connection settings, wireless networks and VPNs as Qt objects, with a QML module,
`org.kde.networkmanager`. Plasma's network applet and settings page,
[plasma-nm](#port-plasma-nm), are built on it.

## Flags

- `-D BUILD_EXAMPLES=OFF` — Build none of the example programs. It is upstream's default, written
  out so the recipe states it.

## Notes

The configure requires `libnm` 1.4 or newer, whose headers give the setting names and enumerations
the wrapper uses, and GIO, through pkg-config: [networkmanager](#port-networkmanager),
[glib](#port-glib) and [pkgconf](#port-pkgconf) are in `depends` for them. At run time the library
talks to the NetworkManager daemon over the system bus, and without it every device list is empty.
The source carries no translations, so the recipe has no `KF_SKIP_PO_PROCESSING`.
