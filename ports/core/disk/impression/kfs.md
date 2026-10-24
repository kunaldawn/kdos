## About
[Impression]{.idx} writes a disk image to a USB drive or a memory card, to make a bootable drive:
it lists the removable drives it finds through [udisks2](#port-udisks2), and can also download an
image of a distribution to write. It is a GTK 4 and libadwaita application written in Rust,
whose meson build runs cargo itself, from the vendor bundle that is the second `sha256 =` line
(see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `OPENSSL_NO_VENDOR=1` — tell the `openssl-sys` crate to link the system [openssl](#port-openssl)
  and never compile a copy of its own, even where a crate asks for its `vendored` feature. The
  HTTPS downloads go through `reqwest` with its `native-tls` feature, which is OpenSSL.
- `-Dprofile=default` — the release profile: meson passes `--release` to cargo, and the
  application id carries no `Devel` suffix. The other choice, `development`, builds cargo's debug
  profile. It is upstream's default, named.

## Notes
The vendor bundle is unpacked at the top of the source tree, beside `Cargo.toml`, not under
`build/`. Meson runs cargo from inside its build directory and sets `CARGO_HOME` to `build/cargo-home`
itself (`src/meson.build`), so a `CARGO_HOME` exported by the recipe would be overridden, and this
recipe, unlike a plain cargo build, sets none. Cargo finds the bundle's `.cargo/config.toml`, the
file that points every crate at `vendor/`, by searching upward from the directory it runs in.

The recipe has no `readelf` check that the program needs `libssl.so`, so a build that compiled
OpenSSL in would not stop.

`build.sh` renders the scalable icon to PNGs at three sizes and writes the desktop entry.
