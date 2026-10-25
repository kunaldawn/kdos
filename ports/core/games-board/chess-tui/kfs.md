## About

chess-tui is a chess game drawn in a terminal, written in Rust. It plays a second player at the
same keyboard, a second player over TCP, any UCI chess engine, or an opponent on Lichess. KDOS
pairs it with [stockfish](#port-stockfish): the desktop entry the recipe writes starts it as
`chess-tui -e /usr/bin/stockfish`, because chess-tui has no default engine, and a game against the
computer with none configured stops at "Chess engine path not configured". A path given with
`-e` is also written into the player's `~/.config/chess-tui/config.toml`.

The build is the plain Rust shape of
[writing-ports.md](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust), with every switch
canonical, and the crates come from the vendor bundle described below.

## Notes

The crates are not downloaded at build time. `ports/fetch` runs `cargo vendor` against the release
and packs the result into `chess-tui-vendor-<version>.tar.xz`, whose `sha256 =` line in the recipe
holds it like any other source; `build.sh` unpacks it and builds with `--frozen --offline` (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

The default feature set is kept, and it is the sound: moves play through the `rodio` crate, which
reaches ALSA through `cpal`, so [alsa-lib](#port-alsa-lib) is a dependency and PipeWire answers the
ALSA device.

The Lichess mode makes HTTPS requests through `reqwest` on rustls, whose crypto is AWS-LC compiled
into the binary from the vendored `aws-lc-sys` crate. No package database sees that copy, so it has
a row in `src/system/kdos-tools/secdb/bundled.txt` that names its version; a bump of the vendor
bundle that moves the crate changes the row in the same change (see
[Bundled copies](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#bundled-copies)). Only the
Lichess mode and the TCP game need a network; the others work without one.
