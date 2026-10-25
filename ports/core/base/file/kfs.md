## About
`file` names what a file is from its contents, using the magic database in
`/usr/share/misc/magic.mgc`; `libmagic` is the same engine as a library.

It is one of the first ports `30_foundation` installs, ahead of every shelf, because builds run it
without naming it: libtool's configure calls `file` to tell a shared library from a static one.
On the finished system it is what the `less` input filter asks what a file is (`file -L -s -b --mime`, see
[Shell environment](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/configuration.md#shell-environment)),
and [nano](#port-nano), [yazi](#port-yazi), [recoll](#port-recoll) and
[e2fsprogs](#port-e2fsprogs) are among the ports that depend on it.

## Flags
- `--enable-zlib`, `--enable-bzlib`, `--enable-xzlib`, `--enable-zstdlib` — Let `file -z` look
  inside gzip, bzip2, xz and zstd compressed files through the libraries, in-process, instead of
  running an external decompressor. Named, a missing library stops configure rather than dropping
  the format.
- `--enable-libseccomp` — Confine `file` to the system calls it needs with a seccomp filter, through
  [libseccomp](#port-libseccomp). The filter is on by default when `file` runs; `file -S` turns it
  off.
- `--enable-landlock` — Also confine it with Landlock, the kernel's file system sandbox, which the
  KDOS kernel has (`CONFIG_SECURITY_LANDLOCK=y` in `kdos.config`).
- `--disable-lzlib`, `--disable-lrziplib` — No lzip or lrzip decompression through a library.
  Neither `lzlib` nor `liblrzip` is in the tree; the [lzip](#port-lzip) port is the program alone.

## Notes
The sandbox and `-z` meet in one place: with the seccomp filter on, `file` can decompress only
through the linked libraries, because the filter forbids running another program. A compressed
format with no library here needs `file -S -z` to be looked into.
