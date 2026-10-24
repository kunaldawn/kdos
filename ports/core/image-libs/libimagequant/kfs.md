## About

libimagequant is the palette quantiser behind `pngquant`: it reduces a 24- or 32-bit image to at
most 256 colours with alpha, for a small 8-bit PNG or GIF. It is written in Rust, and its C library,
`libimagequant.so` with `libimagequant.h` and `imagequant.pc`, is what pngquant, libgd, libvips,
Pillow and Tux Paint link.

## Flags

- `cargo cinstall` — Build and install the crate as a C library with [cargo-c](#port-cargo-c), which
  writes the shared library, the header and the `pkg-config` file in the layout a C consumer
  expects. `--prefix`, `--libdir` and `--destdir` place them as a configure script's equivalents
  would.
- `--manifest-path imagequant-sys/Cargo.toml` — Build the `imagequant-sys` workspace member, which
  holds the C API; the top of the tree is the Rust crate alone.
- `--no-default-features` — With `--features threads`, build with the multithreaded quantiser
  (rayon) and nothing else. `threads` is what the default features amount to; naming it keeps the
  feature set in the recipe.
- `--library-type cdylib` — Build only the shared library; the crate also declares a static one.

## Notes

The Rust crates come from the vendor bundle, `libimagequant-vendor-<version>.tar.xz`, which
`ports/fetch` writes with `cargo vendor` and pins with its own `sha256 =` line (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).
`build.sh` unpacks it at the top of the tree and runs cargo from there: cargo finds the bundle's
configuration by walking up from the current directory, so it must stand where the bundle was
unpacked and reach the C crate through `--manifest-path`, or every crate resolves as missing (see
[Where the bundle
goes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).

The C library carries its own version, `[package.metadata.capi.library]` in
`imagequant-sys/Cargo.toml`, and its soname comes from that, not from the release number: cargo-c
names a `0.x` library by its major and minor numbers (`libimagequant.so.0.<minor>`), so a change of
that minor number needs every consumer rebuilt.
