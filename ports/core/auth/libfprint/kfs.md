## About

libfprint is the [fingerprint reader]{.idx} driver library of the fprint project: it talks to the
USB and SPI sensors it knows, captures and matches prints, and gives one API over all of them. It does
nothing by itself in KDOS; `fprintd`, the daemon that stores enrolled prints, links it, and
`pam_fprintd` from that port is what `sudo`'s PAM stack consults before the password.

## Flags

- `-Dudev_rules=enabled`, `-Dudev_rules_dir=/usr/lib/udev/rules.d` — Install
  `70-libfprint-2.rules`, which binds the SPI sensors in the driver set, matched by their ACPI
  ids, to the kernel's `spidev` driver so the library can open them, and name its directory
  rather than read it from the `udevdir` variable of `udev.pc`. `enabled` installs the file
  whatever the driver set; at `auto` it is installed only when a driver in the set needs udev,
  which the default set's `elanspi` does.
- `-Dudev_hwdb=enabled`, `-Dudev_hwdb_dir=/usr/lib/udev/hwdb.d` — Install the hwdb file that marks
  supported readers for USB autosuspend. Upstream's own description says the entries are
  "included in systemd v248 and later", and on `auto` meson leaves the file out whenever `udev.pc`
  reports a version of 248 or more; systemd is the one place this system will never get them
  from, so the option is `enabled`, not left to a version comparison. Without
  it a reader is not autosuspended and holds the USB bus awake, so a laptop does not reach its
  deeper idle states.
- `-Dintrospection=false` — No GObject Introspection data. With it off, the driver tests are
  skipped, which is what the patch below concerns.
- `-Dgtk-examples=false`, `-Ddoc=false`, `-Dinstalled-tests=false` — No GTK example programs, no
  API reference and no installed tests.

## Patches

- `meson-foreach-dict.patch` — With introspection off, `tests/meson.build` walks the `drivers_tests` dictionary with one loop variable, which meson rejects at setup ("Foreach expects exactly 2 variables"). The patch gives the loop the two variables the introspection branch of the same file already uses.

## Notes

- **The driver set is upstream's default.** `-Ddrivers` is not given. Narrowing it is a bet on
  which sensor a machine turns out to have, and losing it is silent: the reader is not detected,
  which reads as broken hardware.
- **The hwdb is text until the image step compiles it.** eudev reads only the compiled trie,
  `/etc/udev/hwdb.bin`, which kpkg's hwdb trigger and the image build rebuild from every
  `hwdb.d`; see
  [Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes).
