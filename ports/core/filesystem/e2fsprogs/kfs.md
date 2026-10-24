## About

e2fsprogs is the toolkit for the ext2, ext3 and [ext4]{.idx} filesystems: `mke2fs` (and
`mkfs.ext4`), `e2fsck`, `resize2fs`, `tune2fs`, `debugfs`, `fuse2fs` and the rest, with the
libraries `libext2fs`, `libe2p`, `libcom_err` and `libss`. ext4 is the default filesystem
`kinstall` formats an installed root with, and the one `kdos persist` makes a persistence store
on, so `mkfs.ext4` must be present on the ISO; see
[The filesystem table](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kinstall.md#the-filesystem-table).
`script/phases/70_image/090_initramfs.sh` copies `e2fsck` and its three libraries into the
initramfs, which checks an ext2, ext3 or ext4 root with `e2fsck -p` before mounting it
read-write; see
[Checking the root](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#checking-the-root).

## Flags

- `--bindir=/bin`, `--with-root-prefix=""` — `--bindir` puts every user program (`chattr`,
  `lsattr`, `fuse2fs`, `compile_et` and the rest) in `/bin`. The empty root prefix puts the
  programs and libraries e2fsprogs keeps under its root prefix, `e2fsck`, `mke2fs`, `resize2fs`,
  the `fsck.ext*` and `mkfs.ext*` links and the shared libraries among them, in `/sbin` and
  `/lib`. Programs such as `e4defrag` and `filefrag` still go to `/usr/sbin`. In the KDOS root
  `/bin`, `/sbin` and `/lib` are links into `/usr`, so the files land in `/usr/bin`, `/usr/sbin`
  and `/usr/lib` all the same; the package's file list records them under the short paths.
- `--enable-elf-shlibs` — Builds the libraries as ELF shared libraries, which configure does not
  do unless asked. `e2fsck` in the initramfs and every port that links `libext2fs` use them.
- `--disable-libblkid`, `--disable-libuuid` — Use util-linux's `libblkid` and `libuuid` instead of
  building e2fsprogs' private copies. [util-linux](#port-util-linux) installs both, and two
  packages cannot own the same library.
- `--disable-uuidd` — Builds no `uuidd`, the UUID daemon; util-linux provides one.
- `--disable-fsck` — Builds no `fsck` wrapper. The `fsck` front end on KDOS is util-linux's.
- `--enable-fuse2fs` — Builds `fuse2fs`, which mounts an ext filesystem image in user space
  through [fuse](#port-fuse), without the kernel's own ext4 driver.
- `--with-libarchive` — Lets `mke2fs -d` populate a new filesystem from a tar archive as well as
  from a directory. Given as plain `--with-libarchive`, configure has `mke2fs` load libarchive
  with `dlopen` at run time rather than link it;
  [libarchive](#port-libarchive) is in `depends` for it.
- `--with-udev-rules-dir=/lib/udev/rules.d` — Where `64-ext4.rules` and `96-e2scrub.rules` go,
  the rules directory the [eudev](#port-eudev) port uses.
- `--without-crond-dir` — Installs no cron table for `e2scrub_all`. There is no cron daemon on
  KDOS; periodic jobs are `snooze` lines under `/etc/kdos/timers.d`.
- `--without-systemd-unit-dir` — Installs no systemd units for `e2scrub`. KDOS has no systemd.

## Notes

`make install-libs` after `make install` installs the libraries' headers, static archives and
`pkg-config` files. Upstream's top-level `install` target already ends by running `install-libs`
(configure comments that line out only for `--disable-subset`), so the second command installs
the same files again and changes nothing.

The initramfs copies `e2fsck` only when `libext2fs`, `libcom_err` and `libe2p` are all in
`/usr/lib`, and otherwise prints a note and leaves it out; an ext4 root is then mounted without
the check. A change here that renames those libraries, or makes `e2fsck` link another of its own,
has to be matched in `090_initramfs.sh`.
