## About

oxipng is a lossless PNG optimiser written in Rust: it re-encodes the same pixels with better
filter choices and better deflate compression, and the result decodes to exactly the same image.
Because nothing is lost, it is safe to point at a whole archive of screenshots or scanned pages:
nobody has to judge an acceptable loss of quality, as with the lossy
[pngquant](#port-pngquant) on the same shelf.

## Flags

- `--bin oxipng` — Build only the crate's `oxipng` binary target.
- `--manifest-path xtask/Cargo.toml` — Run the `xtask` helper crate, whose `mangen` task renders
  the manual page, `oxipng.1`, from the same `clap` definition of the options that the program
  parses, so the page and the options cannot disagree.

## Patches

- `source-date-epoch.patch` — In `xtask/src/main.rs`, dates the manual page from `SOURCE_DATE_EPOCH` when it is set, and from the current time only when it is not. Without it the page carries the day it was built, and the package's bytes differ on every rebuild.

## Notes

The crates are not downloaded at build time. `ports/fetch` runs `cargo vendor` against the release
and packs the result into `oxipng-vendor-<version>.tar.xz`, held by its `sha256 =` line like any
other source (see [Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).
`xtask` is a crate of its own beside the main one and resolves against its own lock file, so the recipe's
`vendorsync = xtask/Cargo.toml` makes `ports/fetch` pass that manifest to `cargo vendor --sync`
too, and the one bundle serves both `cargo` runs. Without it the offline `cargo run` fails on the
first crate only the xtask needs. oxipng is the example the vendoring section uses for
[`vendorsync`](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes).
