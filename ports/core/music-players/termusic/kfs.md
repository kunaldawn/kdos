## About

[termusic]{.idx} is a music player for the terminal, drawn with ratatui. It is two programs: the
daemon, `termusic-server`, owns the audio device and the playback queue, and `termusic`, the
interface, talks to it over gRPC. Shipping only the interface would give a player that starts and
cannot play. It keeps its own library database and knows nothing about mpd, so the two stacks do not
meet. The recipe writes a terminal desktop entry for it, named Music.

## Flags

- `--features` — Add four features to the symphonia backend: `termusic-server/rusty-libopus`,
  an Opus decoder, which symphonia lacks, linked against the [opus](#port-opus) port;
  `termusic-server/rusty-soundtouch`, playback speed without a pitch shift, through the SoundTouch
  library the `soundtouch` crate compiles from its bundled sources; `termusic-server/rusty-simd`,
  symphonia's SIMD decode paths; and `termusic/cover-viuer-sixel`, album art on a sixel terminal,
  beside the kitty and iTerm protocols that are the interface's defaults. symphonia, a pure-Rust
  decoder, is always compiled in; the GStreamer and libmpv backends are the server's `gst` and `mpv`
  features, and the server's default feature set is empty, so neither is built. A workspace root
  takes features as `crate/feature`.
- `LIBCLANG_PATH=/usr/lib` — Tell bindgen, which generates the SoundTouch bindings at build time and
  loads libclang to do it, where libclang is; `clang` is a dependency for this.

## Notes

The crates come from the vendor bundle, `termusic-vendor-<version>.tar.xz`, pinned with its own
`sha256 =` line and unpacked at the top of the tree; cargo builds `--frozen --offline` from it (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

The workspace asks for rusqlite's `bundled` SQLite. The chroot exports
`LIBSQLITE3_SYS_USE_PKG_CONFIG=1`, which makes libsqlite3-sys link the system library anyway (see
[rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust)),
and `build.sh` checks with `readelf` that both binaries NEED `libsqlite3.so.0`, so a build that fell
back to the bundled copy stops instead of shipping it.

Its network code uses rustls with the default AWS-LC provider, compiled in by `aws-lc-sys`; the copy
is listed in `/usr/share/kdos/bundled.txt` for `kdos cve`, see [Bundled
copies](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#bundled-copies).
