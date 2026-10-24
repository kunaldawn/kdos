## About

basu is [sd-bus]{.idx}, systemd's D-Bus client library, cut out of the systemd tree so that it
builds and runs without systemd. It installs the sd-bus headers under `<basu/…>`, such as
`<basu/sd-bus.h>`, and programs link it as `libbasu` through `basu.pc`; in KDOS that is the compositor `kdos-comp`, the panel `kdos-shell`,
`kdos-record`, both portal backends (`xdg-desktop-portal-kdos` and `xdg-desktop-portal-wlr`),
`fprintd`, `libei` and `cmus`. KDOS has [no systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd),
so this is the sd-bus every one of them gets.

## Flags

- `-Daudit=disabled` — Leave out `libaudit`, the Linux audit library, which is not a port.
- `-Dlibcap=enabled` — Link libcap, through which sd-bus reads a peer's effective, permitted,
  inheritable and bounding capabilities into its credentials (`sd_bus_creds_has_effective_cap()`
  and the rest); without it those queries have nothing to answer from. `enabled` makes a missing
  libcap an error rather than a quieter library.

## Notes

`meson setup` runs `getent passwd 65534` and `id -u nobody` and warns that the local `nobody`
user has UID 99, not the 65534 the `nobody-user` option expects. The check only prints that
warning; the library built is the same.
