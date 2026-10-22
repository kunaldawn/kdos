## About

at-spi2-core is the accessibility layer of the Linux desktop: the accessibility bus, a D-Bus bus
of its own on which applications publish the tree of what they show; the registry daemon; the
`libatspi` client library that screen readers use; and ATK with its bridge to the bus, which GTK
uses. Orca reads GTK and Qt applications through it. The bus is started on demand, by D-Bus
activation of `org.a11y.Bus` the first time something asks for it (see
[The environment a box receives](../../../../docs/kdos/03-architecture/session.md#the-environment-a-box-receives)
and [Accessibility](../../../../docs/kdos/02-user-guide/accessibility.md)).

## Flags

- `--libexecdir=/usr/lib` — Install the bus launcher and the registry daemon, which only D-Bus
  activation starts, under `/usr/lib` rather than in a separate `libexec` directory.
- `-Duse_systemd=false` — Do not hand the bus to a systemd user unit; KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-Ddefault_bus=dbus-daemon` — Run the accessibility bus with `dbus-daemon`. Upstream's default
  is dbus-broker, which needs systemd and is not a port.
- `-Ddbus_daemon=/usr/bin/dbus-daemon` — The path the bus launcher executes. Named, it is compiled
  in rather than looked up on the build machine.
- `-Dgtk2_atk_adaptor=false` — Do not build the module GTK 2 loads; there is no GTK 2 in the tree.
- `-Dx11=enabled` — Build the X11 support, and fail the configure without it: the launcher then
  publishes the bus address as the `AT_SPI_BUS` property on the X root window, where an X client
  under Xwayland finds it, and the device layer can synthesise input for X clients through XTest.
- `-Dintrospection=enabled` — Generate the GObject introspection data, `Atspi-2.0.typelib` among
  it, through which Orca, a Python program, uses `libatspi`.
- `-Ddbus_glib=disabled` — Do not integrate with dbus-glib's main loop. It is upstream's default,
  and dbus-glib is not a port.
- `-Ddocs=false` — No API reference. It is upstream's default.

## Notes

meson looks for libXres without requiring it and builds the X resource support when it finds the
library, so [libXres](#port-libXres) is in `depends` to keep that from following build order.
