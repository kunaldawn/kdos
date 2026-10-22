## About

dosfstools creates, checks and labels FAT filesystems: `mkfs.fat`, `fsck.fat` and `fatlabel`.
[EFI system partitions]{.idx term="EFI system partition"} are FAT, so KDOS needs it wherever it writes one: `kinstall`
formats the ESP of a new install with `mkfs.vfat -F 32 -n KDOS_EFI`, and its first page marks
the install as failing preflight when `mkfs.vfat` is missing; `kdos rebuild` checks for
`mkfs.fat` before it makes a new medium. [libblockdev](#port-libblockdev) and
[gparted](#port-gparted) depend on it too.

## Flags

- `--enable-compat-symlinks` — Also installs the tools under their older names, as links:
  `mkfs.vfat`, `mkfs.msdos` and `mkdosfs` to `mkfs.fat`; `fsck.vfat`, `fsck.msdos` and `dosfsck`
  to `fsck.fat`; and `dosfslabel` to `fatlabel`. They are off by default, and `kinstall` runs
  `mkfs.vfat` by that name, so without them it finds no FAT formatter. `fsck -t vfat` also looks
  for `fsck.vfat`.
- `--docdir=/usr/share/doc/$name` — Where the documentation goes. It is the same directory as
  configure's default, `/usr/share/doc/dosfstools`.
