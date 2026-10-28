## About

LibrePCB draws schematics and lays out printed circuit boards around managed libraries: every
symbol, package and component is a versioned library element with a unique identity. It exports
Gerber and Excellon for fabrication and STEP for 3D. The interface is Qt 6; parts of it are
written in [Slint](https://slint.dev/) and a Rust core, compiled into the same program through
Corrosion.

## Flags

- `-C target-feature=-crt-static` — Link the Rust code against the shared C library. The musl
  target links statically unless told otherwise (see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the programs and libraries with no `RPATH`; every
  library they load is in `/usr/lib`, where the loader looks anyway (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-DBUILD_DISALLOW_WARNINGS=OFF` — Do not compile with `-Werror`. It is upstream's default.
- `-DUSE_GLU=ON` — Include the features that use the OpenGL Utility Library, from the
  [glu](#port-glu) port. It is upstream's default, and configure stops if GLU is not found.
- `-DUSE_OPENCASCADE=ON` — Include the features that use [opencascade](#port-opencascade), which
  reads and writes STEP models. It is upstream's default.
- `-DUNBUNDLE_POLYCLIPPING=ON` — Use the [polyclipping](#port-polyclipping) port instead of the
  copy of the Clipper library in the source. Off by default upstream.

## Patches

- `offline-defaults.patch` — Changes two defaults in the workspace settings: automatic library updates (`library_updates`) from `Install` to `Disabled`, and the live part information lookup (`autofetch_live_part_information`) from on to off. Both ask `api.librepcb.org` without being prompted. Downloading a library on request still works, and both settings can be turned back on in the program.

## Notes

The source is a zip, because upstream publishes no tarball with its submodules. It arrives in
`$SRC` whole, and `build.sh` unpacks it itself before patching.

Two Cargo workspaces are compiled, the Rust core in `libs/librepcb/rust-core` and Slint's C++
interface in `libs/slint`, and one vendor bundle, `librepcb-vendor-<version>.tar.xz`, serves both
lock files. `ports/fetch` cannot write it, because it unpacks only tarballs; it is made by hand
with `cargo vendor --sync libs/slint/Cargo.toml` in `libs/librepcb/rust-core`, run by the fetch
container's cargo (see
[A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes)). Its
`sha256 =` line is its identity, so a version bump means rebuilding the bundle and publishing it
with `ports/publish librepcb`.

The bundle is unpacked into `$SRC_ROOT`, not into the source tree. Cargo reads its configuration
from `CARGO_HOME`, which `build.sh` sets to `$SRC_ROOT/.cargo`, and resolves the bundle's relative
vendor directory against that directory's parent.

`liblibrepcbslint.so` carries an `RPATH` of Qt's library directory despite
`-DCMAKE_SKIP_INSTALL_RPATH=ON`, and the package lint warns about it: the vendored `qttypes`
crate's build script passes it to the linker on every Unix target and has no switch, and editing
a vendored crate fails its checksum file. Qt's library directory is
`/usr/lib`, the loader's default, so the entry changes nothing that loads.

Only the English interface catalogues are kept; `build.sh` deletes the rest from
`/usr/share/librepcb/i18n`.
