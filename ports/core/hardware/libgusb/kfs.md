## About

libgusb wraps [libusb](#port-libusb) in GObject: a `GUsbContext` that enumerates devices and
signals hot-plug, and `GUsbDevice` objects with synchronous and asynchronous transfers. Its
consumer is [libfprint](#port-libfprint), whose fingerprint reader drivers are written on it. The package carries
the GObject Introspection typelib, the Vala API and the `gusbcmd` tool.

## Flags

- `-Dtests=false` — No self tests, which drive real hardware or need umockdev, not a port. The
  `umockdev` option only matters when tests are built.
- `-Ddocs=false` — No API reference, which needs gi-docgen.
- `-Dintrospection=true` — The `GUsb-1.0` typelib and GIR, through
  [gobject-introspection](#port-gobject-introspection); upstream's default, stated.
- `-Dvapi=true` — The Vala API, through [vala](#port-vala); upstream's default, stated. It
  requires introspection.
- `-Dusb_ids=/usr/share/hwdata/usb.ids` — Upstream's default, stated: the vendor and product name
  database from [hwdata](#port-hwdata), which `g_usb_device_get_vid_as_str()` and its siblings read
  at run time.

## Notes

With introspection on, the build runs `contrib/generate-version-script.py` over the GIR to check
the exported symbols against the shipped `libgusb.ver`. The script imports `pkg_resources`, so
[python3-setuptools](#port-python3-setuptools) is a build dependency.
