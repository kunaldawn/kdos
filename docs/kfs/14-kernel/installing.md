# Installing the kernel

Installing the [linux](#port-linux) package puts the kernel in `/boot` and its modules under
`/lib/modules`, but neither is yet something a machine boots. The port's `postinstall.sh`, which
kpkgadd runs after the files are in place, does the rest: it removes module trees nothing can boot
any more, indexes the new modules, builds an [initramfs]{.idx} that matches the new kernel, and, on a
running installed system, copies the kernel and that initramfs to the [ESP]{.idx term="EFI system partition"}. This chapter takes the
script in order. How the two root slots and the ESP fit together is in
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection); this chapter
assumes it.

## How the script is run

`kpkg` packages `postinstall.sh` into the package as a hook, with the recipe's metadata in front of
it, so the script can read `$version`. kpkgadd runs the hook once the package's files are
installed, with `PKG_ROOT` set to the root it installed into. A hook that fails is reported as a
warning and does not undo the install.

`$version` is also the name of the module directory, `/lib/modules/$version`, which `uname -r`
reports once the kernel runs. That holds because `kdos.config` sets no `CONFIG_LOCALVERSION` and
the source is a release archive rather than a git tree, so nothing is appended to the release
string; `build.sh` and this script both depend on it.

## The root and the ESP

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=15-16 pin=d35e37221d3a -->

Every path the script touches is under `$root`, the root the package went into. On a running
system that is `/`, which the second line turns into an empty string so that `"$root"/lib` reads
`/lib`. `kdos update` installs into the inactive slot instead, mounted elsewhere, and working on
`/` then would strip the running system's modules and index the wrong tree.

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=24-24 pin=9e4783b0f9df -->

The ESP is always the running system's, mounted at [`/boot/efi`]{.idx}, whichever root the package is
going into: there is one ESP, and both slots boot from it. `kinstall` puts slot A's kernel and
initramfs in `EFI/kdos/a/`, and `kdos-bootctl deploy` puts each slot's in its own directory. An ESP
whose [boot menu]{.idx} points at a flat pair in `EFI/kdos/` itself, which either slot boots, is read
too:

```text
/boot/efi/EFI/kdos/a/vmlinuz              slot A's kernel
/boot/efi/EFI/kdos/a/initramfs.cpio.gz    and the initramfs built with it
/boot/efi/EFI/kdos/b/vmlinuz              slot B's, once an update has deployed one
/boot/efi/EFI/kdos/b/initramfs.cpio.gz
/boot/efi/EFI/kdos/vmlinuz                a flat pair, on an ESP set up without per-slot directories
/boot/efi/EFI/kdos/initramfs.cpio.gz
```

[One kernel per slot](../../kdos/03-architecture/boot-and-init.md#one-kernel-per-slot) explains
the layout and the boot menu that goes with it.

## Reading a kernel's version

To know which module trees are still needed, the script must know which kernels are on the ESP,
and a file called `vmlinuz` says nothing about its version. The kernel image does:

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=29-40 pin=6d0daeb8e2d9 -->

The x86 boot protocol puts a setup header near the start of every bzImage, and that part of the
file is never compressed:

- Offset 514 (0x202) holds the magic `HdrS`. A file without it is not a bzImage, and the function
  prints nothing.
- Offset 526 (0x20E) holds `kernel_version`, a two-byte little-endian value that `od -tu2` reads.
  Zero means the image carries no version string.
- Otherwise the string is at that value plus 0x200, that is plus 512, from the start of the file.
  It begins with the release, as in `<release> (kdos@kdos) #1 …`, so the script cuts at the first
  NUL and then at the first space.

The loop collects the versions of the flat kernel and of both slots' kernels, those that exist,
into `esp_versions`. Where [`/boot/efi/EFI/kdos`]{.idx} holds no kernel, as in the build chroot, the list
stays empty.

## Pruning old module trees

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=48-58 pin=fc482b3ae53d -->

Each kernel upgrade installs a new `/lib/modules/<version>/`, and nothing else removes the old
one. The script removes every module tree under the root except three kinds:

- **The new kernel's own**, `$version`.
- **The running kernel's**, when the root is the running system. Its modules must keep loading
  until the reboot.
- **Every kernel's on the ESP**, in every root. Until this root's own new kernel reaches the ESP,
  the next boot of this root runs one of those kernels, and a root stripped of its modules boots
  with no GPU, network or sound driver and no message saying why.

Each tree removed is named on a `Removing stale kernel modules:` line. Inside the chroot during the
KDOS build, `uname -r` is the build machine's kernel, which has no tree in the target root, so only
the new kernel's tree survives.

## The module index

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=60-65 pin=41e843eabb41 -->

`build.sh` installed the modules with `DEPMOD=/bin/true`, so the package holds no `modules.dep`
and no other index. `depmod -b` writes them for the new version, in the root the package went
into rather than in `/`. Without the index [`modprobe`]{.idx} finds no module by name, and udev loads
nothing.

## The new kernel's initramfs

The image ships one initramfs, `/boot/initramfs.cpio.gz`, built by `70_image` for the image's own
kernel. Its modules are of that kernel's version and do not load into any other, so a new kernel
needs an initramfs of its own. The script does not build one from nothing. It keeps the image's
initramfs as the base and appends a second archive with the new kernel's copies of the same
modules:

- **The base stays first** because it carries the processor [microcode]{.idx}, which the kernel's early
  loader finds only at the very start of the file, and the init script itself. See
  [Microcode](../../kdos/03-architecture/boot-and-init.md#microcode) and
  [The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs).
- **The appended archive** holds `lib/modules/$version/`. The kernel unpacks concatenated archives
  in order into one tree, and the init's `modprobe` looks under `uname -r`, so it finds the new
  set beside the old one.

### Which modules

The set is the image's own list, `/boot/initramfs.modules`, which the image's initramfs step writes
beside the initramfs. A root without that file takes the set the base itself carries, read out of
the archive:

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=82-107 pin=0139d6ba7d9d -->

The front of the base is one or more uncompressed `newc` cpio archives, the microcode, and the
rest of the file is a gzip-compressed archive. The `while` loop walks the uncompressed ones header
by header. Each `newc` header starts with the magic `070701` and has the size of the file's data at
offset 54 and the size of its name at offset 94, both as eight hexadecimal digits; the name follows
the 110-byte header, and the name and the data are each padded to four bytes. After the archive
whose last entry is `TRAILER!!!`, the loop skips the zero words that pad it and starts again. When
no header follows, `off` is the start of the compressed part, which `gzip -dc | cpio -t` lists.
Every `lib/modules/*/….ko*` path becomes a module name.

Without a set, the new initramfs would hold no module of the new kernel's version, and the init
could load none of the drivers it carries as modules. With this configuration those include f2fs,
the device mapper under LUKS and LVM, and software RAID, so a root on any of them would not mount.
vfat and xfs, which the script's comment also names, are built into this kernel and need no
copy.

### Building the archive

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=109-151 pin=7e8a1c726f2b -->

`modprobe -d "$root" -S "$version" --show-depends` prints, for each module in the set, an `insmod`
line for it and for every module it needs, in the order they load, as found in the new kernel's
tree in the target root. A module built into the kernel prints a `builtin` line instead, which the
`read` loop skips. Each module file is copied into a scratch tree under the same relative path,
decompressed if it is a `.zst`. The three files depmod reads besides the modules,
`modules.order`, `modules.builtin` and `modules.builtin.modinfo`, are copied too, and `depmod -b`
indexes the scratch tree on its own. The tree is written as a `newc` archive, compressed with
`gzip -9`, and appended to a copy of the base.

### Keeping it, or removing the old one

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=156-163 pin=95bc937213d2 -->

The result is written beside the target and renamed over [`/boot/initramfs-kdos.cpio.gz`]{.idx}, so a
failure part-way never leaves a truncated file under the real name. A build that fails removes the
previous `/boot/initramfs-kdos.cpio.gz` as well, because it belongs to the kernel this package has
just replaced. `kdos-bootctl deploy` then finds only the image's initramfs, refuses it beside a
kernel whose modules it does not carry, and the slot keeps the kernel it had until an initramfs is
built.

During the KDOS build itself, `60_kernel` runs before `70_image` has made the image's initramfs,
so the first build of a tree prints the `No /boot/initramfs.cpio.gz` message and the warning that
follows it. That is expected. A later rebuild of the kernel in the same tree finds the image's
initramfs and builds one; `110_iso.sh` removes `/boot/initramfs-kdos.cpio.gz` from the tree again,
so the medium carries only the kernel and initramfs built together.

## Putting the kernel on the ESP

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=170-182 pin=4b79b322d608 -->

The last step depends on the root the package went into:

| Installed into | What happens |
|---|---|
| Another root, such as the inactive slot during `kdos update` | Nothing yet. The script prints the `kdos-bootctl deploy` command; `kdos update apply` runs it once every package of the run is in, so the slot's kernel never reaches the ESP beside a half-updated root |
| The running system, with `/boot/efi` mounted and `EFI/kdos` on it | `kdos-bootctl deploy /` copies the kernel and initramfs into the ESP directory of the slot this root is. The next boot runs the new kernel |
| The running system, with no ESP mounted, as on the live medium or in the build chroot | Nothing |
| The running system, without `kdos-bootctl` | A warning that the new kernel is not on the ESP |

If `deploy` fails, the warning says that the next boot runs the kernel it ran before. What
`deploy` checks before it writes anything is in
[A new kernel](../../kdos/03-architecture/boot-and-init.md#a-new-kernel) and
[the kdos-bootctl section](../../kdos/03-architecture/boot-and-init.md#kdos-bootctl).

## See also

- [Building the kernel](building.md): what the package contains
- [A new kernel](../../kdos/03-architecture/boot-and-init.md#a-new-kernel): the same steps, from the boot path's side
- [One kernel per slot](../../kdos/03-architecture/boot-and-init.md#one-kernel-per-slot): the ESP layout and the menu
- [kdos update](../../kdos/04-programs/kdos-command.md#kdos-update): the update that installs into the inactive slot

<!-- book-nav -->
---

*Part XIV. The kernel, chapter 338.* Previous: [337. Building the kernel](building.md) · [Contents](../README.md) · Next: [Part XV. The image](../15-image/intro.md)
