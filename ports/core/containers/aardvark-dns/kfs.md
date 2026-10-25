## About
aardvark-dns is the DNS server of a podman network: netavark starts it for networks that have DNS
enabled, and it answers containers' queries for each other's names, `A` and `AAAA` records, on the
bridge's own address, forwarding any other query to the configured resolvers. Podman finds it as a helper, at
`/usr/libexec/podman/aardvark-dns`.

## Patches
- `musl-close-range.patch` — At start aardvark-dns closes every file descriptor it inherited above standard error with `libc::close_range()`. The Rust `libc` crate declares that function for glibc targets only and not for musl, so the patch makes the same call as a raw system call, `libc::syscall(libc::SYS_close_range, …)`, which the crate does declare for musl.

## Notes
aardvark-dns is built from a vendor bundle, `aardvark-dns-vendor-<version>.tar.xz`, made by
`ports/fetch` with `cargo vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring). The patch touches only
aardvark-dns's own `src/main.rs`, not a vendored crate, so cargo's checksums of the vendored files
are unaffected.
