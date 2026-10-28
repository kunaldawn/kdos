## About

GNU cpio copies files into and out of cpio and tar archives. KDOS needs it because the
[initramfs](../../../../docs/kdos/03-architecture/boot-and-init.md#the-initramfs)
is a cpio archive in the `newc` format. `script/phases/70_image/090_initramfs.sh` writes it with
`cpio -o -H newc --reproducible`, together with the uncompressed microcode archive that has to
come first in the file, and the [linux](#port-linux) port's `postinstall.sh` lists and rewrites it
when a kernel is installed, which is why `linux` depends on this port.

## Flags

- `--with-rmt=/usr/libexec/rmt` — Builds no `rmt` of cpio's own and compiles in
  `/usr/libexec/rmt` as the remote tape server cpio starts for an archive on another host. That
  file is installed by the [tar](#port-tar) port, whose source carries the same `rmt` from GNU's
  shared paxutils code. Without the switch cpio would install its own `/usr/libexec/rmt` and
  `rmt.8`, paths the tar package already owns, and kpkgadd refuses a path another package owns.
