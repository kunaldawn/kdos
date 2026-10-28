## About
`kdos-pack` builds, inspects, signs, indexes and diffs packs, the filesystem images, signed or
unsigned, that the import lane installs applications from. It is installed on every machine, at
`/usr/bin/kdos-pack`, because `kdos-appbox` runs it on the machine itself: `kdos-box freeze` captures a working box's
writable layer as a pack, and exporting a set of applications packs each one and indexes the
result. The
[kdos-tools](#port-kdos-tools) port depends on it. Its commands are in
[Building a pack](../../../docs/kdos/03-architecture/packs-and-boxes.md#building-a-pack)
and the format it writes in
[A pack is an image with parts appended](../../../docs/kdos/03-architecture/packs-and-boxes.md#a-pack-is-an-image-with-parts-appended).

It links nothing beyond the C library. The file system image is made by running `mkfs.erofs` from
[erofs-utils](#port-erofs-utils), and deltas by running `zstd --patch-from` from
[zstd](#port-zstd), each through an argument vector and never a shell; those two programs are why
the port depends on them, not anything it links.

## Flags
- `-std=gnu11`, `-D_GNU_SOURCE` — The C dialect, which the tree's `CFLAGS` already names, and the
  macro that exposes the C library's GNU and POSIX extensions.
- `-I"$LIBS/libkbase"`, `-I"$LIBS/libksig"`, `-I"$LIBS/libkpkg"`, `-I"$LIBS/libkpack"` — The
  headers of the four libraries it is built from. `$LIBS` is `src/libs`.
- `"$LIBS"/libkbase/*.c`, `"$LIBS"/libksig/*.c`, `"$LIBS"/libkpkg/*.c`, `"$LIBS"/libkpack/*.c` —
  Compile the libraries into the binary, which is how every `libk*` library is used; see
  [How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
  libkpack is the pack format, libksig the signatures and keyring, and libkpkg the version
  comparison kpkg uses as well.
- `"$LIBS"/libksig/monocypher/*.c` — Monocypher, the Ed25519 implementation libksig signs with,
  vendored verbatim under `libksig`; it is the reason for the `BSD-2-Clause OR CC0-1.0` in the
  licence line. See [libksig](../../../docs/kdos/05-developer/c-libraries.md#libksig).
- `-O2`, `-Wall`, `-Wextra` — The tree's optimisation level, named again, and the compiler's
  common and extra warnings.

## Notes
A pack built twice from the same tree must be the same bytes, or a delta between two releases
means nothing and a second builder cannot be checked against the first. `main.c` passes
`mkfs.erofs` a set time (`SOURCE_DATE_EPOCH`), set owners, a 4096-byte block size and a UUID
derived from the pack's id, and each of those is a source of drift if it is removed; see
[Reproducibility](../../../docs/kdos/03-architecture/packs-and-boxes.md#reproducibility).

The span the pack digest covers is set by the footer's format number. Widening it without a new
format number makes every pack already in use fail its hash check, so no box composes. Each pack
is verified by the format it declares, and `kdos-pack restamp` raises an older pack to the current
format, dropping its signatures, which then have to be made again.
See [Rules the format keeps](../../../docs/kdos/03-architecture/packs-and-boxes.md#rules-the-format-keeps).
