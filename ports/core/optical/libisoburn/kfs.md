## About
libisoburn joins [libisofs](#port-libisofs), which lays out an ISO 9660 file system, and
[libburn](#port-libburn), which drives an optical drive, into one library, and installs
`xorriso` on top of them: a single program that builds, extends and burns ISO images, with
`xorrisofs`, `xorrecord` and `osirrox` as its other personalities.

KDOS needs it twice. The image step, `script/phases/70_image/110_iso.sh`, runs
`xorriso -as mkisofs` to write `kdos.iso`, the hybrid image that boots from optical media and
from a USB stick ([Packaging: `70_image`](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/how-kdos-is-built.md#packaging-70_image)),
and `kdos rebuild` checks for `xorriso` before it promises an ISO. On the desktop, `kdos-burn`
in [kdos-shell](#port-kdos-shell) writes a folder or an image to a disc by running `xorriso` as
the logged-in user, and [k3b](#port-k3b) runs `mkisofs` from `$PATH` to build a data image.

`build.sh` adds `mkisofs` and `genisoimage` as links to `xorriso`. Started under one of those
names, `xorriso` reads `.mkisofsrc` and takes its arguments in `mkisofs` style, so a program that
expects cdrtools' or cdrkit's command finds a compatible one.

## Flags
- `--disable-debug` — Upstream's configure defaults to a debug build, which adds `-g -pedantic
  -Wall -Wextra` and `-DDEBUG`. Disabling it adds `-DNDEBUG`, `-fexpensive-optimizations` and an
  `-O3` that sits in front of the tree's `CFLAGS`, so the tree's later `-O2` is the level that
  applies. The [libburn](#port-libburn) and [libisofs](#port-libisofs) ports pass the same switch.
- `--enable-libreadline` — Give `xorriso`'s interactive dialog mode line editing and history with
  [readline](#port-readline). The probe falls back without an error when readline is missing, so
  readline in `depends` is what keeps it.
- `--disable-libedit` — Do not fall back to libedit when readline is not found.
- `--enable-libacl` — Read and record POSIX ACLs of the files put into an image, with
  [acl](#port-acl), and restore them when `xorriso` extracts files from an image. Like readline,
  a probe that turns the feature off quietly if acl is missing.
- `--enable-zlib` — Build the zisofs and gzip content filters, which compress a file inside the
  image, with [zlib](#port-zlib); a probe of the same kind.
- `--disable-libjte` — Build without libjte, which writes the Jigdo template and checksums of an
  image as it is made. libjte is not in the tree; libisofs is built `--disable-libjte` as well.
- `--disable-libcdio` — Drive optical drives through libburn's own Linux SCSI code rather than
  libcdio. It is upstream's default on Linux; libburn is built with the same switch.
- `--enable-external-filters` — Compile in `-external_filter`, which registers a program such as
  `bzip2` to transform file content on its way into the image. It is upstream's default, and the
  command stays banned when `xorriso` runs setuid.
- `--enable-launch-frontend` — Compile in `-launch_frontend`, which starts a front end program,
  such as the installed `xorriso-tcltk`, connected to `xorriso` over two pipes. It is upstream's
  default, and banned under setuid as well.
- `-j1` — Run the install with one job, overriding the tree's parallel `MAKEFLAGS` for that one
  invocation, as the [libburn](#port-libburn) and [libisofs](#port-libisofs) ports do; see
  [A parallel install race](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#a-parallel-install-race).

## Notes
readline, acl and zlib are all probes that switch their feature off without a word when the
library is missing, so the `depends` line is what keeps line editing, ACLs and zisofs in this
build: removing a dependency there changes the program and the build still succeeds.

A change to how `xorriso` lays out the volume is a change to the boot medium. `110_iso.sh`
checks the finished image for an EFI system partition, an MBR boot signature and a non-empty MBR
boot code area after `limine bios-install` has run, and fails the build if any is missing,
because `xorriso` accepts a boot option it does not implement without an error.
