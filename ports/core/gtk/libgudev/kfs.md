## About

libgudev wraps `libudev` in GObject: a GLib program uses it to enumerate devices, read their udev
properties and receive add, change and remove events on the GLib main loop. On KDOS `libudev` is
[eudev](#port-eudev)'s, so no systemd is involved. Its users are the system services and desktop
libraries built on GLib, among them [upower](#port-upower), [networkmanager](#port-networkmanager),
[modemmanager](#port-modemmanager), [udisks2](#port-udisks2) and the GStreamer plugin packages.

## Flags

- `-Dintrospection=disabled` — Generate no `GUdev-1.0` GIR or typelib. At the default, `auto`, the
  data would be generated whenever gobject-introspection is installed first.
- `-Dvapi=disabled` — Generate no Vala binding, which needs `vapigen`.
- `-Dtests=disabled` — Build no tests. They need umockdev, which is not a port.
