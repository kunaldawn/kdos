## About

cargo-c adds `cargo cbuild`, `cargo cinstall`, `cargo ctest` and `cargo capi` to cargo. They build
a Rust crate that exports a C interface as a C library would be shipped: a versioned shared
library, a static archive, a generated C header and a pkg-config file, installed under the usual
paths. In KDOS the ports that use it are `librsvg`, `libimagequant` and `libdovi`, C libraries
written in Rust that other C programs link.

cargo-c links cargo as a library, so like `cargo` itself it links libgit2, libssh2, libcurl and
OpenSSL from their ports. The source is a vendor bundle of every crate it needs
(`cargo-c-vendor-<version>.tar.xz`, `vendoring = rust`), so the build is offline; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

## Flags

- `LIBSSH2_SYS_USE_PKG_CONFIG=1` — makes the `libssh2-sys` crate look for the system libssh2
  through pkg-config; without it the crate compiles the copy of libssh2 it carries.
- `LIBGIT2_NO_VENDOR=1` — makes `libgit2-sys` link the system libgit2, and fail if it is missing,
  instead of compiling its bundled copy. `script/env/chroot.env` exports this for every phase from
  `40_lang` on; cargo-c is built in `31_compilers`, so the recipe exports it itself (see
  [Writing ports](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
- `-C target-feature=-crt-static` — link the four programs dynamically against musl's `libc.so`
  and the system libraries; the musl target links statically unless told otherwise (see
  [Writing ports](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
- `--root=$PKG/usr` — where `cargo install` puts what it installs: the four programs go to
  `$PKG/usr/bin`.

## Notes

`cargo install` also writes `.crates.toml` and `.crates2.json` into its root, its record of what it
has installed there. `build.sh` deletes both, so the package holds the four programs and the licence
file only.