## About

libgudev wraps `libudev` in GObject: a GLib program uses it to enumerate devices, read their udev
properties and receive add, change and remove events on the GLib main loop. On KDOS `libudev` is
[eudev](#port-eudev)'s, so no systemd is involved. Its users are the system services and desktop
libraries built on GLib, among them [upower](#port-upower), [networkmanager](#port-networkmanager),
[modemmanager](#port-modemmanager), [udisks2](#port-udisks2) and the GStreamer plugin packages.

## Flags

- `-Dintrospection=disabled` — Generate no `GUdev-1.0` GIR or typelib. gobject-introspection is
  not in `depends`, so at the default, `auto`, whether the package carried the data would depend on
  whether gobject-introspection happened to be installed before this port was built.
- `-Dvapi=disabled` — Generate no Vala binding. It needs `vapigen` from vala, which is not in
  `depends` either, and `auto` would make the package depend on build order in the same way.
- `-Dtests=disabled` — Build no tests. They need umockdev, which is not a port.
