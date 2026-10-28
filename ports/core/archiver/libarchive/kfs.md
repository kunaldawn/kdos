## About
libarchive reads and writes tar, cpio, zip, 7-Zip, ISO 9660, xar and many other archive formats,
through any of the common compression filters, and installs four programs built on it: `bsdtar`,
`bsdcpio`, `bsdcat` and `bsdunzip`. The system `tar` stays GNU tar; the BSD programs sit beside it
under their own names.

Two things in KDOS lean on it directly. [cmake](#port-cmake) links the shared library for
`cmake -E tar` and its packaging, so libarchive and every library it depends on are installed early
in `30_foundation`, ahead of cmake in the order run at the head of the phase's list, well before
the `archiver` shelf's section. And the file preview in [kdos-shell](#port-kdos-shell) decides whether a file is an
archive by asking libarchive to open it, and lists its entries if it can (see
[kdos-peek](../../../../docs/kdos/04-programs/kdos-shell.md#kdos-peek)).
Desktop programs such as [ark](#port-ark), [gvfs](#port-gvfs) and [mpv](#port-mpv) use it too.

## Flags
- `--enable-bsdtar`, `--enable-bsdcpio`, `--enable-bsdcat`, `--enable-bsdunzip` — Build and install
  the four command-line programs. Each is upstream's default; the switches name what the package
  installs. With the bare value `yes` and the static library still built (`--disable-static` is not
  passed), configure links libarchive into each program statically, so `bsdtar` and the others
  carry their own copy and do not load `libarchive.so`; `readelf -d /usr/bin/bsdtar` names the
  compression libraries but not libarchive.
- `--enable-acl` — Read and restore POSIX ACLs when archiving and extracting, through
  [acl](#port-acl).
- `--enable-xattr` — Read and restore extended attributes. configure looks for `setxattr()` in
  the C library first, and musl has it, so nothing from [attr](#port-attr) is linked.
- `--enable-posix-regex-lib=libc` — Take the POSIX regular expressions of `bsdtar -s` (the
  path-rewriting option) from musl's `regcomp`, and never look for the PCRE or PCRE2 POSIX
  libraries. configure falls back to a separate `libregex` only when the C library has no
  `regcomp`, which musl has.
- `--with-zlib`, `--with-bz2lib`, `--with-lzma`, `--with-zstd`, `--with-lz4` — gzip, bzip2, xz and
  lzma, zstd and lz4 filters, through [zlib](#port-zlib), [bzip2](#port-bzip2), [xz](#port-xz),
  [zstd](#port-zstd) and [lz4](#port-lz4).
- `--with-lzo2` — The `lzop` filter, through [lzo](#port-lzo). Unlike the others this one is off
  unless asked for.
- `--with-openssl` — Take MD5, SHA and RIPEMD-160 for mtree and xar checksums, and the ciphers for
  encrypted zip archives, from OpenSSL's `libcrypto`.
- `--without-nettle`, `--without-mbedtls` — Do not take those hashes from Nettle or Mbed TLS. Both
  are off by upstream's default, and both libraries are ports here; with the switches stated,
  OpenSSL is the only crypto library libarchive links.
- `--with-expat` — Parse xar archives' XML table of contents with [expat](#port-expat).
- `--without-xml2` — Do not use libxml2 for xar. configure prefers libxml2 when it finds it, and
  only libxml2 provides the XML writer, so with this switch libarchive reads xar archives (through
  expat) and refuses to write them with "Xar not supported on this platform".
- `--without-libb2` — No external BLAKE2 library; RAR 5 checksums use the copy of BLAKE2 libarchive
  carries in its own source. No libb2 port exists.

## Notes
configure looks for zlib, bzip2, xz, zstd, lz4 and OpenSSL whether or not they are named (LZO only
when asked), and when one is missing it builds without it and says so only in its output:
`--with-zstd` and the rest do not make a missing library fatal. What guarantees them is the
recipe's `depends` line, which installs every one of them first. A library dropped from `depends`
would leave a `libarchive.so` with no built-in support for that filter, and no failed build to show
for it; libarchive would then run the external program instead (`zstd`, `xz` and the like), with a
warning. `readelf -d /usr/lib/libarchive.so` lists the libraries a build really linked.
