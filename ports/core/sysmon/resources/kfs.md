## About

Resources is a GNOME system monitor written in Rust on GTK 4 and [libadwaita](#port-libadwaita):
it graphs CPU, memory, GPU, NPU, disks, network and battery, and lists and manages applications
and processes. KDOS's own monitor is [kdos-res](../../../../docs/kdos/04-programs/kdos-res.md);
this is the GTK one beside it. Its Rust crates come from a vendor bundle, and meson drives the
cargo build.

## Flags

- `-Dprofile=default` — the release profile. Upstream's default, `development`, builds cargo in
  debug mode and gives the program the application id `net.nokyan.Resources.Devel`; `default`
  passes `--release` to cargo and keeps the plain id `net.nokyan.Resources`, which the desktop
  entry and its `StartupWMClass` name.

## Patches

- `fix_amd_npu_libc_type.patch` — In `src/utils/npu/amd.rs`, the AMD NPU query, changes the return type of the `ioc()` and `iowr()` helpers that build ioctl request numbers from `libc::c_ulong` to `i32`. The Rust `libc` crate declares musl's `ioctl()` with an `int` request, where glibc's takes an `unsigned long`, so the code does not compile on musl without it.

## Notes

The vendor bundle, `resources-vendor-<version>.tar.xz`, is made by `ports/fetch` with
`cargo vendor` (`vendoring = rust`, see
[vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)) and checked against its
`sha256 =` line like any source. meson builds the program with one `cargo build` of the top
manifest. The `process_data` crate in `lib/process_data`, which the program depends on by path, has
a lock file of its own, read by the `cargo test` target meson declares for it; `vendorsync` names
that crate's `Cargo.toml`, so `cargo vendor --sync` puts the crates of both lock files into the one
bundle.

meson runs cargo with `CARGO_HOME` set inside the build tree, so cargo never reads a
`config.toml` from a cargo home. It finds the bundle's `.cargo/config.toml` instead by its upward
search from the build directory, which is why `build.sh` unpacks the bundle at the top of the
source tree (see
[where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).

Ending another user's process and reading memory-module details through `dmidecode` run through
`pkexec`, which [polkit](#port-polkit) answers; both are in `depends`. Upstream installs its icon
as SVG only, so the recipe rasterises it to PNGs, and writes its own desktop entry named
`Resources (GNOME)`, because `Resources` is the name of kdos-res's entry.
