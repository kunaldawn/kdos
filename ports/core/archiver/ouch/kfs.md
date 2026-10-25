## About
[ouch]{.idx} compresses and decompresses with one command whatever the format: `ouch compress`,
`ouch decompress` and `ouch list` choose the format from the file name, for tar, zip, 7z,
gzip, bzip2, bzip3, xz, lzma, lzip, lz4, zstd, brotli and snappy; rar it only decompresses and
lists, since RAR's licence allows no more. It is a Rust program built with cargo
from its vendor bundle, the second `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

It is built with upstream's default features, which include `unrar` and `bzip3`. Each of those
compiles a C or C++ library carried inside its crate, so both formats are part of the binary,
which is why it links `libstdc++`; neither library is a port of its own.

## Flags
- `-C target-feature=-crt-static` — link against musl dynamically, the rule for every cargo build
  in the tree (see [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)). Here it is
  also what lets the build run at all: the `libbzip3-sys` crate's build script generates its
  bindings with bindgen, which loads `libclang` at build time, and a statically linked musl
  program cannot load a shared library. [clang](#port-clang) is in `depends` for the same reason.
- `LIBCLANG_PATH=/usr/lib` — where bindgen's `clang-sys` looks for `libclang.so`, so it does not
  have to search.
- `OUCH_ARTIFACTS_FOLDER="$PWD/artifacts"` — read by ouch's `build.rs`: when set, it writes the
  manual pages and the shell completions into that directory, which `build.sh` then installs.
  Unset, the build writes neither.

## Notes
The zstd format links the system library: from `40_lang` on the tree exports
`ZSTD_SYS_USE_PKG_CONFIG=1`, which makes the `zstd-sys` crate find [zstd](#port-zstd) through
pkg-config instead of compiling its own copy (see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)). The `readelf` line after
`cargo build` checks that the binary needs `libzstd.so.1`, so a crate that falls back to its copy
fails the build instead of shipping one.
