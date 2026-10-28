## About

resvg is an SVG renderer written in Rust, with no browser engine behind it. The port installs its
command, `resvg`, which renders an SVG file to a PNG; the Rust library and its C API are not
installed. [yazi](#port-yazi), the terminal file manager, names it as the program it previews SVG
files with.

## Flags

- `-p resvg` — Build only the `resvg` crate of the workspace, the one with the command-line
  program. The workspace's other members are the C API (`crates/c-api`), `usvg`, the SVG parser and
  simplifier the renderer is built on, and `usvg`'s code generator.

## Notes

The crates are not downloaded at build time. `ports/fetch` runs `cargo vendor` against the release
and packs the result into `resvg-vendor-<version>.tar.xz`, whose `sha256 =` line in the recipe holds
it like any other source; `build.sh` unpacks it and builds with `--frozen --offline` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). The rest of the build is
the plain Rust shape of
[writing-ports.md](../../../../docs/kdos/05-developer/writing-ports.md#rust).
