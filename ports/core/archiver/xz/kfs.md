## About

XZ Utils is the `.xz` and `.lzma` format: the library `liblzma` and the `xz`, `unxz`, `xzcat`
and `lzma` commands, with the `xzgrep`, `xzdiff` and `xzless` scripts. On KDOS it is the package
format itself. kpkg rolls every package as a `.tar.xz`, piping the archive through `xz -T0` on all
cores, and unpacks every `.tar.xz` source with it (see
[What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
`liblzma` is linked by [file](#port-file), [libarchive](#port-libarchive), [elfutils](#port-elfutils),
[zstd](#port-zstd), [gdb](#port-gdb) and many more.

Until `30_foundation` the root's `xz` is the one [Part III](#03-bootstrap-050-xz) built, outside any
package; this port, the second of the archiver shelf, replaces it and adopts its files.

## Flags

- `${CFLAGS/-O2/-O3}` — raise the exported `CFLAGS` and `CXXFLAGS` from `-O2` to `-O3` before
  configure reads them. `-O3` is raised per port, never across the tree (see
  [One release flag set, raised per port](../../../../docs/kdos/01-philosophy/decisions.md#one-release-flag-set-raised-per-port)).
- `--enable-nls` — build translated messages, which is configure's default. The catalogues the
  install writes are removed again by kpkg's language rule, so the package carries none (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `--enable-sandbox=landlock` — use Landlock, the Linux sandbox a program can impose on itself, to
  restrict what `xz` may do once it has parsed its arguments. Writing only to standard output, it
  gives up creating or removing files; with exactly one input file, it gives up opening anything
  else once that file is open. Left at `auto`, configure would pick Landlock when a test program
  using `linux/landlock.h` and the Landlock system calls links, and silently build no sandbox when
  it does not; named outright, a failed test stops configure with `landlock support not found`.

## Notes

Landlock cannot be combined with `-fsanitize`. A sanitizer build cannot compress or decompress a
single file to standard output, so configure stops with an error when `CC` or `CFLAGS` contain
`-fsanitize=`; such a build of this port needs `--disable-sandbox` in place of the switch above.
