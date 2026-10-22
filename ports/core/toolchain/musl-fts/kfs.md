## About

[fts]{.idx} is the BSD and glibc interface for walking a directory tree (`fts_open`,
`fts_read`, `fts_close`): a program gets each file in turn with its `stat` already filled in.
musl does not provide it. musl-fts is the NetBSD implementation packaged for musl, and it installs
`libfts.so`, the header `fts.h`, a `musl-fts.pc` file for pkg-config and the `fts(3)` manual page.

`elfutils` is the port in this phase that needs it: `libdw.so` links `libfts.so.0`, so every
program that reads DWARF through elfutils loads it too. `btrfs-progs`, `exfatprogs`, `libzip`,
`clamav`, `dolphin` and `ft2-clone` also name it.

## Flags

- `./bootstrap.sh` — the source is GitHub's archive of a tag, which has no generated `configure`.
  Upstream's script runs `libtoolize`, `aclocal`, `autoconf`, `autoheader` and
  `automake --add-missing` to produce it.

## Notes

`Makefile.am` installs the library, the header and the `.pc` file but not `fts.3`, which sits at
the top of the source; `build.sh` installs it by hand. The `man-pages` port deletes its own
`fts.3`, since two packages cannot own one path, and keeps `fts_open.3`, `fts_read.3` and the
rest, each a one-line `.so man3/fts.3`; those pages show this port's text, and show nothing when
this package is not installed. The package also carries a static
`libfts.a`, since `configure` is not given `--disable-static`.
