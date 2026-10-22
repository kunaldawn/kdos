## About

Sniffnet is a graphical network monitor written in Rust with the iced toolkit: it captures packets
through [libpcap](#port-libpcap) and shows the traffic grouped by host, service, program and
country. The country and ASN lookups come from databases upstream embeds in the binary. The port
installs the program, its icons and a desktop entry that `build.sh` writes itself, with a bare
`Exec=sniffnet`, a `TryExec` line and the `Network;Monitor;` categories, in place of upstream's
`resources/packaging/linux/sniffnet.desktop`.

## Patches

- `sniffnet-no-update-check.patch` — Makes `set_newer_release_status()` in `src/utils/check_updates.rs` return `None` instead of asking GitHub whether a newer release exists. Sniffnet runs that check when the window starts; with the patch it sends no request to GitHub and shows no release status.

## Notes

The build is offline: `sniffnet-vendor-<version>.tar.xz` holds every crate, produced by `cargo
vendor` when `ports/fetch` sees `vendoring = rust`, and it carries its own `sha256 =` line, so a
regenerated bundle that resolves differently is refused. See
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring).

`reqwest` pulls in rustls, whose crypto provider is `aws-lc-rs` over `aws-lc-sys`, so the binary carries its own copy of AWS-LC. That
copy is listed in `/usr/share/kdos/bundled.txt` with the version the vendored source states, and a
bump that regenerates the bundle must rewrite that row; see
[Bundled copies](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#bundled-copies).

Capturing needs raw-socket privilege, and the port grants none: no setuid bit and no file
capability. Run it as root, or give the binary the capability yourself, as
[setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries)
describes.
