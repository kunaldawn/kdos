# The initramfs

The [initramfs]{.idx} is the small root the kernel unpacks into memory and runs first. Its one job
is to find the real root, on a disk or on the boot medium, and hand over to it.
`script/phases/70_image/090_initramfs.sh` builds it from the finished root in `build/fs`: it lays
out a directory tree, copies into it the programs early boot runs with every library they need,
picks the [kernel modules]{.idx term="kernel module"}, writes the [`init`]{.idx} script, puts the processor [microcode]{.idx} in front, and packs
the lot into one file. This chapter goes through the script in that order. The `init` it writes is
long enough to have [a chapter of its own](091-initramfs-init.md).

What the initramfs does at boot, and why each part is there, is described in
[The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs) and the sections after
it. This chapter is about how the file is made.

**Runs in:** the chroot, as a script step of `70_image`\
**Reads:** the programs, libraries, [udev]{.idx} rules and kernel modules installed in `build/fs`\
**Writes:** the tree `build/initramfs/`, the microcode tree `build/ucode/` and its archive
`build/ucode.cpio`, the finished `build/initramfs.cpio.gz`, and [`/boot/initramfs.modules`]{.idx} in the
root

## The workspace

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=16-29 pin=7415a8924f1b -->

`/kdos` is the repository inside the chroot, so the tree is `build/initramfs` in the checkout. It is
deleted and made again on every run, and nothing from an earlier initramfs survives into the next.
The directories are the mount points `init` uses (`proc`, `sys`, `dev`, `run`, `mnt/iso`, `newroot`)
and the places the copies below go. `etc/passwd` and `etc/group` are the root's own.

The tree is left in place when the step ends. `build/initramfs/init` is the generated script as the
kernel will run it, and `ls build/initramfs/bin build/initramfs/lib` shows what was carried.

## Proving the reader

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=31-58 pin=f70faa51b673 -->

The script does not ask `ldd` which libraries a program needs. It reads them from the program's
dynamic section instead: `readelf -d` prints one `(NEEDED)` line for each library the file was
linked against, and `needed_libs` keeps the name in brackets. Those are the direct needs only;
the libraries of a library are found by reading the library in turn, which is what
[copy_closure](#copy_closure) does.

Before the script trusts that reader, it tries it on a program whose answer it knows. util-linux's
[`blkid`]{.idx} links `libblkid`, so a reader that works must print a `NEEDED` line for it. `readelf` comes
from [binutils](#port-binutils); `llvm-readelf` prints the same lines and is the second choice. If
neither passes, the step stops. The comment says why the test has to be explicit: `needed_libs` runs
inside `$(...)`, where a failing command does not stop a `set -e` script, so a missing reader would
make every program look as if it needed nothing.

## copy_closure

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=60-76 pin=6cc03b22393a -->

`copy_closure DEST SRC` copies one program and then, for each library it names, the library itself,
by calling itself on it. A library already in `lib/` is skipped, so a library that many programs
need is copied once and the recursion ends. Libraries are looked for in `/usr/lib` and then `/lib`.
When one is in neither, the step stops and names the program that wanted it, so a broken [closure]{.idx term="dependency closure"} is
a failed build and never a program that cannot start at boot.

`cp` follows symbolic links, so a library reached through its `.so.N` link is copied as a regular
file under that name, which is the name the program asks the [dynamic linker]{.idx} for.

## The GNU programs

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=90-118 pin=d61f96526b19 -->

The first row is what `init` itself runs; the rest are there for the
[rescue shell](091-initramfs-init.md#the-rescue-shell) and for the udev rules, of which
`63-md-raid-arrays.rules` runs `readlink` and `basename`. Each is looked for in `/usr/bin` and then
`/usr/sbin`, and each must answer `--version` with a GNU package name before it is copied:
`(GNU coreutils)`, `(GNU grep)` and so on, or ` (coreutils) ` for `dd`, the one program of the set
whose banner leaves out the word GNU. A program that is missing, or that is some other userland's,
stops the step. The programs come from [coreutils](#port-coreutils), [grep](#port-grep) and
[sed](#port-sed).

## The util-linux programs

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=78-88 pin=2104fe8437aa -->

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=120-140 pin=7f4c417aa0ff -->

`util_linux_only` runs the program with `--version` and accepts it only if the answer contains
` from util-linux `. [`switch_root`]{.idx}, `mount`, [`umount`]{.idx}, [`losetup`]{.idx}, `dmesg` and [`setsid`]{.idx} are held to
it here and `blkid` a little further down, all from [util-linux](#port-util-linux). The reason for
`switch_root` is given at length in the comment, and in
[switch_root](../../kdos/03-architecture/boot-and-init.md#switch_root): a stand-in boots the machine
and leaves every process chrooted, which only shows when something tries to start a box. Each
program is copied with `copy_closure`, under its own name in `bin/`.

## The splash, the C library and bash

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=142-163 pin=12680665e249 -->

[kdos-splash](#port-kdos-splash) is statically linked, so it is copied alone, with the font it draws
in. When it is not installed the step prints a warning and carries on, and the machine boots without
a splash; every call to it in `init` first tests that `/bin/kdos-splash` exists.

musl's `libc.so` is its own dynamic linker, so the link `ld-musl-x86_64.so.1`, the interpreter every
program in the tree names, points at it. [bash](#port-bash) and the three libraries beside it are
listed by hand rather than walked with `copy_closure`: bash names `libreadline`, which names
`libncursesw`, and `libhistory` comes with them. `bin/sh` is bash under its other name, and `init`
is a bash script. The [closure check](#the-closure-check) at the end of the step reads every program
in the tree, these included, so a library missing from this list stops the build there.

## blkid

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=165-175 pin=13ba4102af85 -->

Every lookup `init` makes by UUID is `blkid -U`, so this is the one program whose failure would stop
every installed machine from finding its root. It is held to util-linux like the programs above.
What each lookup is and what a stand-in breaks is listed in
[blkid](../../kdos/03-architecture/boot-and-init.md#blkid).

## mdadm and e2fsck

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=177-210 pin=8ca658b61abc -->

[mdadm](#port-mdadm) is optional: without it the initramfs cannot assemble an array, and `init`
skips the `RAID` stage. It is copied with its closure (`libudev`) to `usr/sbin/`, the path its udev
rules run, and `bin/mdadm`, which `init` runs, is a link to it.

[`e2fsck`]{.idx} from [e2fsprogs](#port-e2fsprogs) checks an ext root before it is mounted. It is copied
only when its three e2fsprogs libraries are installed, and then with them; otherwise the step prints
a note, `init` finds no `/bin/e2fsck`, and an ext root is mounted unchecked. `e2fsck` also names
`libblkid` and `libuuid`, which this block does not copy: they reach `lib/` with `blkid` and
[`cryptsetup`]{.idx}, and the closure check stops the build if they do not.

## udev

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=212-219 pin=c82a6ed7ed75 -->

`udevd` and `udevadm` come from [eudev](#port-eudev). They name `libkmod`, which names `liblzma`,
`libz` and `libzstd`, and `libblkid`, which `blkid` has already brought. `libudev` is for [`mdadm`]{.idx}.
This list, too, is written out by hand, and the closure check holds it to what the binaries name.

## kdos-bootctl

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=221-246 pin=5789c9f57049 -->

[`kdos-bootctl`]{.idx} decides which root slot to boot when a machine has two; see
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection). It is one name
of the multi-call `ksvc` binary from [kdos-tools](#port-kdos-tools), and `cp` copies the binary
through the link, so the initramfs gets all of it, and with it its `libpng16`. The script looks for
`/usr/bin/kdos-bootctl` and then for `/usr/bin/kdos-tools`. Either the binary and libpng are both
copied, or neither is and the step prints a note; with no `kdos-bootctl`, `init` skips the
`BOOT SLOT` stage and boots the `root=` on the command line.

## cryptsetup

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=248-266 pin=d9663ad1fab1 -->

[cryptsetup](#port-cryptsetup) unlocks an encrypted root. When it is installed it is copied with the
libraries in the list; each library is copied when it exists and skipped quietly when it does not,
and the closure check at the end of the step then stops the build if `cryptsetup` names one that was
skipped. Nothing later in the build asks whether cryptsetup was carried. When cryptsetup
is not installed, `init` still parses `cryptdevice=`, and the unlock fails with
`cryptdevice: this initramfs has no cryptsetup`.

## lvm2

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=268-307 pin=d275004527c8 -->

The test for [lvm2](#port-lvm2) is its package database entry or its [device-mapper]{.idx term="device mapper"} udev rule; on any
image where either is present, the step either carries [LVM]{.idx} or stops. `lvm` and [`dmsetup`]{.idx} must both
exist, and every udev rule that runs one of them must name it under `/usr/sbin`. The second test
reads every rule file, drops comment lines, and collects each path ending in `/dmsetup ` or `/lvm `
that does not start with `/usr/sbin/`; any such path, or a `95-dm-notify.rules` without the
`udevcomplete` call, stops the step and prints what it found. The two programs are then copied with
their closures to `usr/sbin/`, the path the rules name, with links from `bin/` so that `PATH=/bin`
finds them too.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=309-344 pin=8375cc419aef -->

[`thin_check`]{.idx} and [`cache_check`]{.idx} are installed by
[thin-provisioning-tools](#port-thin-provisioning-tools) as links to one program. `readlink -f`
finds it, `copy_closure` copies it once under its own name, and the two names are made links to it
again. The generated `lvm.conf` names those two paths and nothing else. Activation itself is in
[Activating volume groups](../../kdos/03-architecture/boot-and-init.md#activating-volume-groups).

## The udev rules and kmod

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=346-371 pin=8b74918ed89a -->

The rules come from both rule directories, and then the whole of `/usr/lib/udev` is copied, which
brings the helper programs the rules call. `hwdb.d` is removed again: udev reads only the compiled
`hwdb.bin`, and the tree does not carry one. Every `2>/dev/null || true` here means that a missing
directory is not an error. Which rules and helpers stay is decided at the end of the step, in
[the udev check](#the-udev-check).

The rules name the real root's paths: helpers under `/usr/lib/udev` (where udevd also runs a bare
helper name from), programs under `/usr/bin`, `/usr/sbin` and `/sbin`. The links make each of those
paths answer here: `usr/lib/udev` is `lib/udev`, `usr/bin` and `sbin` are `bin`, and
`usr/sbin/blkid` is `bin/blkid`. lvm2's and mdadm's programs are in `usr/sbin` itself.

[kmod](#port-kmod) is one program that behaves as [`modprobe`]{.idx}, [`insmod`]{.idx} and [`depmod`]{.idx} according to the
name it is run under, so those three are links to it.

## Kernel modules

### Finding the kernel

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=373-390 pin=4f1656a3e5d6 -->

The modules belong to the newest kernel in `/lib/modules`, taken with `sort -V`. With none there,
the [linux](#port-linux) port has not been installed and the step stops. `depmod -a` runs only when
that kernel has no `modules.dep`, the index [`modinfo`]{.idx} uses to turn a module's name into its file.

### copy_module

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=392-443 pin=f18c3199a32b -->

`copy_module NAME` does for one module what `copy_closure` does for a program:

1. A name listed in `modules.builtin` is part of the kernel image and has nothing to copy.
2. `modinfo -k <version> -n` prints the module's file. When `modinfo` gives nothing, [`find`]{.idx} looks
   for `NAME.ko*` instead.
3. A module not found at all is a warning, and the list goes on.
4. The file keeps its path relative to `/lib/modules/<version>`. A `.ko.zst` is decompressed on the
   way in and stored as `.ko`; anything else is copied as it is.
5. `modinfo -F depends` lists the modules it needs, and each is copied by the same function. A
   module already in the tree ends the recursion.

### The module list

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=445-484 pin=89cd4623a48c -->

The list grows in groups: the live medium's needs (`overlay`, `squashfs`, `isofs`, the CD drive and
`loop`) with the common storage controllers and USB storage, the disk transports the installer can
install to, `vfat` and its two code pages for the ESP, the root filesystems the installer offers
beyond the built-in [ext4]{.idx} and btrfs, device-mapper and the LUKS ciphers, software RAID, and the
device-mapper targets an LVM root can use. Why each group is there is in
[The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs).

A name in the list that is built into the kernel costs nothing, so the list names what a boot needs
rather than what this kernel happens to build as modules. Two other parts of the tree have to agree
with it. Every root filesystem `ki_filesystems[]` in the installer offers must be in it, and
`testing/preflight.sh` checks that, ext4 and btrfs excepted because they are built in. And
`modprobe` in `init` loads modules by these names, so a module `init` asks for must be in the list
to be in the tree at all.

### The list on the image, and the module index

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=486-507 pin=28265998fce2 -->

The list is written to `/boot/initramfs.modules` in the root, one name per line. The `linux` port's
`postinstall.sh` reads it when a later kernel is installed and builds that kernel's initramfs from
the same set; see [A new kernel](../../kdos/03-architecture/boot-and-init.md#a-new-kernel).

`modules.order` and `modules.builtin` are copied beside the modules, with
`modules.builtin.modinfo` when the kernel has one, and then `depmod -b .` writes the initramfs's own
`modules.dep` and indexes, for the modules it holds and nothing else. When
`/boot/System.map-<version>` exists, `-F` names it, read where the kernel installed it; the tree
does not carry the map. depmod's manual describes the map as what `-e` checks modules' symbols
against, and this call passes no `-e`.

## The init script

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=509-512 pin=1ad9e2d8fa1a -->

`init` is written by `cat` from a here-document that ends at the `EOF` line below. The delimiter is
not quoted, so the shell expands `$` and backquotes inside it while the step runs. Every `$` that
belongs to `init` is therefore written `\$`, and every backquote `` \` ``: `\$ROOT_UUID` in the step
becomes `$ROOT_UUID` in the file. None is left unescaped, so nothing from the build's environment
reaches the script. The next chapter shows it piece by piece as it stands in the step, backslashes
included. `build/initramfs/init` is the same text with those escapes undone, and with every line
that ends in a backslash joined to the next one, which an unquoted here-document also does.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1296-1297 pin=612d39e844dc -->

## Microcode

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1299-1335 pin=2374498df0b8 -->

The processor's early microcode loader runs before the kernel unpacks anything, and reads two
literal paths from the start of the initrd as it is in memory. Those paths are built here in a tree
of their own, `build/ucode`. The Intel file is the bundle the [intel-ucode](#port-intel-ucode) port
builds, copied under the name the loader wants. An empty bundle stops the step, and so does an
installed `intel-ucode` package with no bundle; with neither, Intel processors keep the microcode
their firmware loaded, and the step says so.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1337-1363 pin=1677e9d765a0 -->

The AMD containers come from [linux-firmware](#port-linux-firmware), one per processor family, and
are concatenated into one file in the order the shell's glob sorts them. A `.bin.zst` is expanded
first, because the early loader cannot decompress. A directory that yields an empty file stops the
step. The microcode tree is then packed as a cpio archive of its own, uncompressed, and only when at
least one vendor's file is in it. Why the archive must be uncompressed and first is in
[Microcode](../../kdos/03-architecture/boot-and-init.md#microcode).

## The udev check

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1365-1424 pin=8d290eaeb14f -->

A rule whose program is absent costs nothing in function, since the root's `01_udev` replays every
rule, but udevd logs `failed to execute` for every device the rule matches, early in every boot.
So the step keeps only what can run. A helper in `lib/udev` is read with the same `needed_libs` as
the closure check below, and goes when a library it names is not in `lib/`, as `mtp-probe`
(libmtp) and libinput's and libgpod's helpers do. Then each rules file's `RUN`, `PROGRAM` and
`IMPORT{program}` values are cut down to their first word; an absolute path is looked up in the
tree through the links above, a bare name in `lib/udev`, and anything else (a `$env{}` or a `%`
substitution) is passed over. The first program that is not there removes the whole file,
with a `Note:` line naming both. The eight files in `_required` are the ones a root's assembly
waits on; one of them that would be removed, or that is on the root and not here, stops the step.

## The closure check

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1426-1444 pin=efc9486cc771 -->

This is the check every hand-written list above relies on. Every regular file in `bin/` and `usr/sbin/`, and
every shared library in `lib/`, is read with the reader proven at the start, and every library it names must be in
`lib/`. Links are skipped, because their targets are read under their own names; a file that is not
an ELF binary, such as a script, prints no `NEEDED` lines and passes. Any gap stops the step with
one line per program and library, in the form `bin/cryptsetup:libjson-c.so.5`.

## Packing

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1446-1454 pin=0b7432e9b2d1 -->

The tree is listed with `find`, sorted byte by byte so that the order does not depend on the
filesystem, and written by [cpio](#port-cpio) in copy-out mode (`-o`) in the `newc` format, the one
the kernel unpacks. `--reproducible` is GNU cpio's alias for `--device-independent`: it records no
number for the device a file is stored on, numbers the inodes in archive order from 0, and records every directory with a link count of 2, so
the archive does not depend on the filesystem the tree sits on. It keeps each file's modification
time. [gzip](#port-gzip) `-9` compresses it; reading from a pipe, gzip writes zero in its header's
time field and stores no file name.

The finished `build/initramfs.cpio.gz` is the microcode archive followed by the gzipped one, or the
gzipped one alone when there is no microcode. Despite its name, then, it starts with a plain cpio
archive. The kernel unpacks the two one after the other.

## Contents of the initramfs

**Programs in `bin/`:** `bash` (and `sh`), the GNU programs `cat`, `chmod`, `cut`, `ls`, `mkdir`,
`mknod`, `seq`, `sleep`, `cp`, `mv`, `rm`, `ln`, `rmdir`, `head`, `tail`, `wc`, `stat`, `df`, `dd`,
`sync`, `uname`, `readlink`, `touch`, `basename`, `sort`, `tr`, `grep` and `sed`; util-linux's
`switch_root`, `mount`, `umount`, `losetup`, `dmesg`, `setsid` and `blkid`; `udevd`, `udevadm` and
`kmod` (as `modprobe`, `insmod` and `depmod`); and, each when its package is installed,
`kdos-splash`, `e2fsck`, `kdos-bootctl`, `cryptsetup`, and links to `mdadm`, `lvm` and `dmsetup`.

**Programs in `usr/sbin/`:** `blkid` as a link to `bin/blkid`; when mdadm is installed, `mdadm`;
when lvm2 is installed, `lvm`, `dmsetup`, thin-provisioning-tools' `pdata_tools`, and `thin_check`
and `cache_check` as links to it.

**Links:** `usr/bin` and `sbin` to `bin`, and `usr/lib/udev` to `lib/udev`.

**Other files:** `init`; `lib/` with musl and every library above; `lib/udev/` with the rules and
helpers that passed the udev check; `lib/modules/<version>/` with the modules and their index;
`etc/passwd`, `etc/group`, `etc/udev/rules.d/` and, with lvm2, `etc/lvm/lvm.conf`;
`usr/share/kdos/splash.psf` with the splash.

## See also

- [The initramfs init](091-initramfs-init.md): the script this chapter writes, stage by stage
- [The ISO](110-iso.md): where `initramfs.cpio.gz` goes next
- [The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs): what it carries and
  why
- [Microcode](../../kdos/03-architecture/boot-and-init.md#microcode): the early loader's rules
- [Tools that must be util-linux's](../../kdos/03-architecture/boot-and-init.md#tools-that-must-be-util-linuxs)
- [A new kernel](../../kdos/03-architecture/boot-and-init.md#a-new-kernel): how an installed machine
  rebuilds it

<!-- book-nav -->
---

*Part XV. The image, chapter 353.* Previous: [352. The image gates](087-gates.md) · [Contents](../README.md) · Next: [354. The initramfs init](091-initramfs-init.md)
