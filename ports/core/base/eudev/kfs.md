## About
eudev is the [device manager]{.idx}: `udevd` receives the kernel's device events, creates and
names the nodes and links under `/dev`, sets their owners and permissions from its rules, loads
the driver a device needs, and tells the rest of the system through libudev. It is a fork of
systemd's udev that builds without systemd, and KDOS uses it in place of `systemd-udevd` (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
The `01_udev` boot script starts it, the initramfs carries `udevd` and `udevadm` with the rules
(see
[The initramfs](../../../../docs/kdos/03-architecture/boot-and-init.md#the-initramfs)),
and libudev is what the session's input, storage, audio, Bluetooth and camera stacks link to find
their devices.

KDOS's own rules are the files in `fs/etc/udev/rules.d`, which the file-system overlay places
in `/etc/udev/rules.d`. The hardware database is compiled once over the finished tree rather than
by this port (see
[Databases stamped into the image](../../../../docs/kdos/05-developer/build-system.md#databases-stamped-into-the-image)).

## Flags
- `--enable-manpages` — Install the manual pages `udev(7)`, `udevadm(8)`, `udevd(8)`,
  `udev.conf(5)` and `hwdb(7)`. They are off by default; the source ships them already formatted,
  so this needs no XSLT tool.
- `--bindir=/sbin`, `--sbindir=/sbin` — Install `udevadm` and `udevd` in `/sbin`, which is a link to
  `/usr/sbin`. With the two directories equal, the install makes no second `udevadm` link in
  `sbindir`.
- `--libexecdir=/lib` — A directory nothing in eudev's build uses: its helpers go to the root
  libexec directory, which follows `--with-rootlibdir`.
- `--with-rootprefix=` — An empty root prefix. The build passes it to the compiler as `ROOTPREFIX`,
  which no source file reads; its effect is on configure's split-`/usr` test, which a root prefix other than the
  default and other than `${prefix}` turns on. `udevd` then also reads rules from
  `/lib/udev/rules.d` and `/usr/lib/udev/rules.d`, and finds a helper a rule names without a path
  in `/usr/lib/udev` or `/lib/udev`. On KDOS's merged layout each pair is one directory, the one
  the rules and helpers are installed in.
- `--with-rootlibdir=/lib` — Put `libudev.so.1` in `/lib`, and with it the helpers and rules in
  `/lib/udev`. `--libdir=/usr/lib` still places the development link `libudev.so`, `libudev.a` and
  `libudev.pc` under `/usr/lib`.
- `--enable-blkid` — Build udev's `blkid` builtin, which probes each block device for its file
  system type, label and UUID and gives the rules `ID_FS_*` properties, the source of the
  `/dev/disk/by-uuid` and `/dev/disk/by-label` links. It links libblkid from
  [util-linux](#port-util-linux), and `yes` makes configure stop when libblkid is missing.
- `--enable-kmod` — Build the `kmod` builtin, through which a rule loads a module for a device's
  `MODALIAS` (`80-drivers.rules` does this for every device that has one). It links libkmod from
  [kmod](#port-kmod), and configure stops when it is missing.
- `--disable-selinux` — Build without libselinux, which is not a port in the tree.

## Patches

`watch-reread-changed-table.patch` changes `synthesize_change()` in `src/udev/udevd.c`, which
runs when a program closes a watched disk it had open for writing (`60-block.rules` puts
`OPTIONS+="watch"` on every `sd*`, `vd*` and `nvme*` disk). Upstream takes the disk's lock and
asks the kernel to re-read its partition table (`BLKRRPART`) on every such close, and the re-read
removes and re-adds every partition node even when the table is the same. The patch first reads
the table on the disk with libblkid, read-only, and compares it with the kernel's partitions by
number, start and size, and an extended partition by its start alone, because the kernel gives
it a size of one or two sectors; when they match it skips the re-read and sends `change` for the disk and
each partition, which re-runs the rules without touching the nodes. A table that differs, or one
libblkid cannot read, is re-read as upstream does. Without it, a program that writes the whole
disk, closes it and then opens one of its partitions finds the node gone while `udevd`'s own
re-read runs: Ventoy's `Ventoy2Disk.sh -I` does that between cleaning its second partition and
formatting the first with `mkexfatfs`, and loses `/dev/sda1` in a live session. The patch also
gives `udevd` libblkid's compiler flags and library in `src/udev/Makefile.am`, which `autoreconf`
carries into the build. It is to be offered upstream to eudev.

## Notes
The source is a GitHub tag archive with no generated `configure`, so `build.sh` runs
`autoreconf -f -i -s` first; the autotools are in the phase's opening run, ahead of every shelf.

eudev needs util-linux's libblkid, so [util-linux](#port-util-linux) is built before it, and
without libudev. The cost is in
[Known gaps](../../../../docs/kdos/06-reference/known-gaps.md#lsblk-shows-a-filesystems-type-label-and-uuid-to-root-only).

`udev.pc` carries `udevdir=/lib/udev`, and a port that installs rules or helpers by asking
pkg-config for that variable puts them there.
