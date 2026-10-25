## About

wayland-protocols is the set of Wayland protocol extensions maintained beside the core protocol:
`xdg-shell`, which every desktop window is, and the rest of the stable, staging and unstable
extensions (`text-input`, `linux-dmabuf`, `presentation-time`, `fractional-scale`, `tablet` and
many more). The package is mostly the protocols' XML, under `/usr/share/wayland-protocols`, with a
pkg-config file, `wayland-protocols.pc`, whose `pkgdatadir` tells a consumer where that is. A
consumer runs [wayland](#port-wayland)'s `wayland-scanner` over the files it needs during its own
build. The one thing this port generates itself is a header of enumerations per protocol,
`/usr/include/wayland-protocols/<protocol>-enum.h`, which it makes with `wayland-scanner --strict`
whenever the scanner is installed, and in KDOS it always is.

In KDOS it is a build dependency of every program that speaks an extension:
[wlroots](#port-wlroots) and [kdos-comp](#port-kdos-comp) on the server side, the KDOS clients of
`src/desktop`, and the Wayland backends of the toolkits, Mesa and Xwayland.

## Flags

- `-Dtests=false` — Builds none of upstream's checks, which compile the scanner's output for every
  protocol. With tests on, configure requires `wayland-scanner`; with them off, it uses the
  scanner when it finds one, for the enumeration headers, and carries on without it otherwise.

## Notes

The `--strict` run makes a protocol file that fails the DTD check stop this build, rather than
warn, so a broken XML file in a new release shows up here and not in the first consumer.

A program built against an older copy of these files does not see a protocol added later, and a
newer protocol version in the XML does nothing until the compositor implements it. The version
that matters at run time is the one [kdos-comp](#port-kdos-comp) and wlroots advertise, not the
one installed here.
