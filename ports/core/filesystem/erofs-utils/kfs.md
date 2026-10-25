## About

erofs-utils builds and inspects [EROFS]{.idx} images, the compressed read-only filesystem the
kernel mounts directly: `mkfs.erofs` makes one, `fsck.erofs` checks and extracts one, and
`dump.erofs` prints its layout. Every KDOS [pack]{.idx} is an EROFS image with its metadata,
icon and signatures appended. `kdos-pack` makes the image by running `mkfs.erofs -zzstd -b 4096`
with a fixed timestamp, owner and UUID, so that a pack depends only on its inputs; see
[Reproducibility](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packs-and-boxes.md#reproducibility).
`script/phases/70_image/100_packs.sh` runs it the same way for the root-filesystem pack. The
[kdos-pack](#port-kdos-pack) and [kdos-packd](#port-kdos-packd) ports depend on this one.

## Flags

- `autoreconf -f -i -s` — The source is a git snapshot from kernel.org with `configure.ac` but no
  `configure`, so the build system is generated first.
- `--sbindir=/usr/sbin` — Where `mount.erofs` goes, the default for `--prefix=/usr`, named here.
  `mkfs.erofs`, `fsck.erofs` and `dump.erofs` install into `/usr/bin`.
- `--enable-lz4`, `--enable-lzma` — Build LZ4 and LZMA compression, from [lz4](#port-lz4) and
  [xz](#port-xz). Both default to `auto`, which would build them only when the library is found.
- `--with-libzstd` — Builds zstd compression, from [zstd](#port-zstd), and makes a missing
  libzstd a configure error where the default, `auto`, would build without it. A pack's image is
  compressed with zstd (`-zzstd`); a `mkfs.erofs` without zstd refuses that compressor and no pack
  can be built.
- `--with-zlib` — Uses [zlib](#port-zlib) to inflate DEFLATE data, so the tools can read a
  DEFLATE-compressed image and `mkfs.erofs` can take a gzip-compressed tar as input. DEFLATE
  compression itself is erofs-utils' own code and is built either way.
- `--enable-multithreading` — Compresses with several threads. It is upstream's default.
- `--without-xxhash`, `--without-libcurl`, `--without-openssl`, `--without-libxml2`,
  `--without-json-c`, `--without-libnl3` — Link none of these libraries. libcurl and json-c are
  what the OCI registry input needs, and libcurl, OpenSSL and libxml2 what S3 image generation
  needs; both features are off unless asked for (`--enable-oci`, `--enable-s3`). libnl3 is the
  generic-netlink library of the NBD backend, and without libxxhash erofs-utils compiles its own
  copy of the hash. Each defaults to `auto`: without the switch configure links the library
  whenever an earlier port has installed it, and the package then needs a library its `depends`
  line does not name.
- `--without-uuid` — Builds without util-linux's libuuid. erofs-utils then generates a UUID with
  `getrandom` and parses one given with `-U` by its own code, so `kdos-pack`'s fixed UUID works
  the same.
- `--without-selinux` — Builds no SELinux file labelling. No libselinux port exists.
- `--disable-fuse` — Builds no `erofsfuse`, which mounts an image in user space through FUSE. The
  kernel's EROFS driver mounts packs, and `kdos-packd` mounts a pack file directly. It is
  upstream's default.
