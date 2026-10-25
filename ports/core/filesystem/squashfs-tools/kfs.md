## About

squashfs-tools makes and unpacks [squashfs]{.idx} images, the compressed read-only filesystem the
KDOS live medium boots from: `mksquashfs` builds one, `unsquashfs` lists and extracts one, and
`sqfstar` and `sqfscat` build one from a tar archive and print files from one.
`script/phases/70_image/110_iso.sh` writes the ISO's `system.sfs` with `mksquashfs`, zstd at
level 15 and 1 MiB blocks unless `KDOS_ISO_COMP` names another codec or level, then appends the recipe
tree as `/ports` and checks the result with `unsquashfs -l`. `kdos rebuild` needs `mksquashfs` to
make a new medium from a source tree on a running system. How the live root is mounted is in
[The live medium and persistence](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence).

The tools have no configure step. The build is selected with variables on the `make` command
line, which override the Makefile's own assignments.

## Flags

- `XZ_SUPPORT=1` — Builds the xz (LZMA2) compressor, from [xz](#port-xz)'s liblzma. It is on by
  default. `110_iso.sh` accepts `KDOS_ISO_COMP=xz`.
- `LZMA_XZ_SUPPORT=1` — Builds the LZMA1 compressor too, through the same liblzma. Upstream
  leaves it off and calls it obsolete in favour of xz.
- `ZLIB_SUPPORT=1` — The Makefile has no variable of this name, so it changes nothing. gzip comes
  from `GZIP_SUPPORT`, which is on by default and links [zlib](#port-zlib).
- `LZ4_SUPPORT=1` — Builds the LZ4 compressor, from [lz4](#port-lz4). It is on by default.
- `ZSTD_SUPPORT=1` — Builds the zstd compressor, from [zstd](#port-zstd), the default codec of
  the ISO image. It is on by default.
- `USE_PREBUILT_MANPAGES=y` — Installs the manual pages shipped in `Documentation/manpages`
  instead of generating them from the built programs' `-help` output with `help2man`.
- `INSTALL_DIR=$PKG/usr/bin`, `INSTALL_MANPAGES_DIR=$PKG/usr/share/man/man1` — Where `make
  install` copies the programs and the pages. The Makefile has no `DESTDIR`; these paths include
  `$PKG` themselves, and its default is under `/usr/local`.
- `SED=/usr/bin/sed` — The `sed` the manual-page scripts use. `install-manpages.sh` sources
  `functions.sh`, which checks that `$SED`, or the first `sed` on `PATH`, behaves as GNU sed and
  stops the install if it does not, prebuilt pages or not. [sed](#port-sed) is in `depends`.

## Notes

LZO (`LZO_SUPPORT`) is also on by Makefile default, and [lzo](#port-lzo) is in `depends` for it.

The prebuilt manual pages list the compressors of upstream's default build, which has no LZMA1, so
they leave out the `lzma` this build adds. `mksquashfs -help` on the installed program is the
accurate list.
