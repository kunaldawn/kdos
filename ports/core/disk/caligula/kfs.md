## About

caligula writes a disk image to a removable drive from the terminal. It offers only removable
devices to choose from unless given `--show-all-disks`, decompresses a gzip, bzip2, xz, LZ4 or
Zstandard image as it writes, shows the progress, and reads the drive back afterwards to verify what
it wrote: a way to put the KDOS ISO, or any other image, on a USB stick from a shell. It is a Rust
program built from a vendored crate bundle, linked against the system's [xz](#port-xz).

## Patches

- `xz2-shared-liblzma.patch` — Changes caligula's `Cargo.toml` dependency on the `xz2` crate from `features = ["static"]` to the plain dependency. The `static` feature makes `lzma-sys` compile the copy of liblzma it carries and link it into the binary; without it, `lzma-sys` finds the system `liblzma` through pkg-config. No environment switch overrides a feature a program turns on, so the change has to be made in the manifest. See [rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust).

## Notes

The `readelf -d` line after `cargo build` stops the build unless the binary NEEDs `liblzma.so.5`, so
a vendored crate that falls back to its own copy of the library fails the build instead of shipping
a private liblzma that no update to the `xz` port reaches. The patch is applied after the vendor
bundle is unpacked; `Cargo.lock` lists crates and not their features, so the bundle serves the
patched manifest unchanged.
