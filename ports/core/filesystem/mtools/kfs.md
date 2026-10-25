## About

GNU mtools reads and writes MS-DOS FAT filesystems without mounting them: `mcopy`, `mdir`,
`mformat`, `mmd` and the rest work on a device or an image file directly, as an ordinary user. In
KDOS its consumer at build time is [limine](#port-limine), whose configure needs `mcopy` to build
`limine-uefi-cd.bin`, the FAT image inside the ISO that UEFI firmware boots from.
[gparted](#port-gparted) depends on it as well.

## Flags

- `--sysconfdir=/etc/default` — Compiles `/etc/default/mtools.conf` in as the system-wide
  configuration file. mtools reads `/etc/mtools.conf` and `/etc/default/mtools.conf` whatever
  this is set to, so the value adds no third path to that list. Each user's file is
  `~/.mtoolsrc`. The port installs no configuration file.
- `--disable-floppyd` — Builds no `floppyd`, the daemon that grants mtools on a remote machine
  access to the local floppy drive, and no `floppyd_installtest`. `floppyd` authenticates its
  clients through an X server and links libX11, and configure builds it whenever X is found.
- `--without-x` — Looks for no X libraries at all. `floppyd` is their only user, and configure
  stops with "Floppyd needs X support" if `floppyd` is asked for while X is off.

## Notes

`floppyd.1` and `floppyd_installtest.1` are in the Makefile's list of manual pages whether the
programs are built or not, so `make install` installs both pages. The recipe deletes them, so the
package carries no page for a program it does not contain.
