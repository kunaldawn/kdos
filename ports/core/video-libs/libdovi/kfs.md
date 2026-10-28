## About
[libdovi]{.idx} is the C library of `dolby_vision`, the Rust crate inside quietvoid's dovi_tool:
it parses, converts and writes the Dolby Vision RPU metadata a video stream carries, and is
installed as `libdovi.so` and the static `libdovi.a`, with the header `libdovi/rpu_parser.h` and
the pkg-config module `dovi`. [HandBrake](#port-handbrake) links it to carry Dolby Vision
through a transcode.

## Flags
- `--features capi` — Turn on the crate's `capi` feature, which enables the optional `libc`
  dependency and the crate's `serde` feature. `cargo cinstall` compiles the C API module whether
  the feature is on or not, and that module imports `libc`, so without the feature the build has
  no `libc` crate; the C functions that take a JSON configuration are compiled only with `serde`.

## Notes
- **Only the library is built.** The tarball is the whole dovi_tool repository; its top-level
  `Cargo.toml` is the `dovi_tool` command, and `dolby_vision/` is the library crate with a manifest
  and `Cargo.lock` of its own. `vendordir = dolby_vision` makes `ports/fetch` vendor that manifest's
  crates, and `build.sh` changes into the directory before unpacking the bundle, so the
  `.cargo/config.toml` it carries is where cargo stands (see
  [Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).
- **cargo-c does the install.** `cargo cinstall` builds the crate as a C shared library and a
  static archive, and installs both with the header and `.pc` file, taking the library name
  `dovi`, the header subdirectory `libdovi` and `-Cpanic=abort` from the crate's
  `[package.metadata.capi]`. The two `test -f` lines stop the build if the library or the header is missing from `$PKG`.
