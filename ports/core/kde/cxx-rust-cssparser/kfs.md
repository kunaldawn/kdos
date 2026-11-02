## About

cxx-rust-cssparser is a C++ library over Mozilla's `cssparser` and Servo's `selectors` crates, the
CSS tokeniser and selector matcher of Firefox's style engine. It parses a style sheet into rules,
selectors and typed property values, and hands them to C++ through a `cxx` bridge. Union, the Qt
style engine that plasma-workspace requires, reads its style sheets with it. The library is
`libcxx-rust-cssparser`, found with `find_package(cxx-rust-cssparser)`; its closure holds no Qt
and no graphics library, so the port is built in `41_system`.

The Rust half is a static library that CMake builds through [corrosion](#port-corrosion) and links
into the shared C++ library, so the port carries a
[Rust vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

## Flags

- `-D CMAKE_INSTALL_LIBDIR=lib` — The project includes `GNUInstallDirs` itself and installs to
  `CMAKE_INSTALL_LIBDIR`; it reads ECM's install directories only for its documentation, which
  is off.
- `-D BUILD_DOCS=OFF` — Build no QDoc documentation. Upstream's default is on, and the build then
  needs Qt's `qdoc`, which this port's `depends` does not name.
- `-C target-feature=-crt-static` — In `RUSTFLAGS`: build the crate for musl's shared C library.
  The musl target otherwise links its C library statically, and the static library Corrosion
  links into `libcxx-rust-cssparser.so` would carry that choice into a library every consumer
  loads beside the shared one; see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).
- `CARGO_NET_OFFLINE=true` — Keep cargo off the network. Every crate resolves from the bundle's
  `vendor/`.

## Notes

The crate's `Cargo.toml` and `Cargo.lock` are in `rust/`, not at the top of the tarball, so the
recipe sets `vendordir = rust` and `build.sh` unpacks the bundle there; Corrosion runs cargo from
that directory, and cargo finds the bundle's `.cargo/config.toml` by walking up from it. The bundle
holds the 61 crates of that lock file. The crates linked into the library bring their licences
into `license`: `cssparser`, `cssparser-color`, `dtoa-short` and `selectors` are MPL-2.0,
`foldhash` is Zlib, and the rest MIT or Apache-2.0; `cxx-build`, `syn` and the other build-time
crates run only during the build.

The example program, `cxx-rust-cssparser-parse`, is built and installed whatever the switches say:
it reads a CSS file and prints what the library parsed, which makes it the quickest check of the
library on an installed system.
