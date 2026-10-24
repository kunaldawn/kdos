## About

wiremix is a terminal mixer for PipeWire: every playback and recording stream, every device, their
volumes, and which device each stream is routed to. ALSA's `alsamixer` sees the card and not
PipeWire's graph, so this is the mixer that shows what is actually playing where. The package adds
a desktop entry, *Volume Mixer*, which opens it in a floating terminal of 80 by 20 cells through the
KDOS keys `X-KDOS-Float` and `X-KDOS-Size` (see
[kdos-term](../../../../docs/kdos/04-programs/kdos-term.md#what-it-is-and-where-it-sits)).

## Flags

- `LIBCLANG_PATH=/usr/lib` — Where bindgen finds `libclang`. The `pipewire-sys` and `libspa-sys`
  crates generate their bindings from PipeWire's headers at build time, and bindgen loads
  [clang](#port-clang)'s library to parse them. The `RUSTFLAGS` line matters here too: with
  `-crt-static` the build script is linked dynamically and can load a library; a static musl
  binary cannot (see [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).

## Notes

The crates come from the vendor bundle, `wiremix-vendor-<version>.tar.xz`, made by `ports/fetch`
with `cargo vendor` and unpacked into the top of the source tree; the build runs
`--frozen --offline` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). The desktop entry is
written by the recipe, as the tree does for terminal programs (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
