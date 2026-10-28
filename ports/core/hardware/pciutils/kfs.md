## About

pciutils is the PCI toolkit: `lspci`, which lists the PCI devices of the machine with their names,
capabilities and the kernel driver bound to each, `setpci`, which reads and writes their
configuration registers, and `libpci`, the library under both. The names come from the PCI ID list,
`pci.ids`, which the [hwdata](#port-hwdata) port installs. The browsers, [powertop](#port-powertop),
[fastfetch](#port-fastfetch) and most of the other ports that name it use `libpci`;
[tlp](#port-tlp) runs `lspci`.

There is no `configure`: every setting is a make variable, kept in the `pci_opts` array and passed
to both `make` lines, so the install step cannot configure a different library from the one that
was built. Left unset, each feature is auto-detected and quietly dropped when its library is
missing; every one is named here.

## Flags

- `SHAREDIR=/usr/share/hwdata` — Look for `pci.ids` in `/usr/share/hwdata`, where hwdata installs
  it.
- `SHARED=yes` — Build `libpci` as a shared library; the makefile's default is a static one.
  `install-lib` on the install line then installs it with its headers and pkg-config file.
- `ZLIB=yes` — Read the ID list through zlib, so a compressed `pci.ids.gz` is read as well as a
  plain `pci.ids`.
- `DNS=yes` — Build `lspci -q`, which looks up a device missing from the local ID list in the
  central PCI ID database over DNS, and caches the answer in `$XDG_CACHE_HOME/pci-ids`, or in
  `~/.cache/pci-ids` when that is unset. It reaches the network only when that option is given.
- `LIBKMOD=yes` — Link [kmod](#port-kmod)'s `libkmod`, so `lspci -k` names the kernel modules able
  to drive each device. Named `yes`, a `libkmod` that pkg-config cannot find stops the build.
- `HWDB=yes` — When the ID list has no name for a device, look it up in udev's hardware database,
  which [eudev](#port-eudev) builds.
- `OPT` — The compile flags the makefile uses, given as the tree's `CFLAGS` with `-fPIC -DPIC`
  added for the shared library.

## Notes

With `ZLIB=yes` the library's default ID file is `pci.ids.gz`, and `make install` installs a copy
of the list under that name. `build.sh` deletes it. When the `.gz` file does not exist, `libpci`
drops the suffix and opens `pci.ids`, so every program reads hwdata's list rather than the copy in
pciutils' own tarball.

The makefile installs `libpci.so.<version>` with mode 644; `build.sh` sets it to 755.
