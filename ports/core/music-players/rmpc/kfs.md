## About
rmpc is a terminal client for [mpd](#port-mpd), the music player daemon, drawn with the ratatui
library and able to show album art in the terminal. It speaks mpd's protocol, so it attaches to a
daemon that is already playing rather than starting one; mpd is in `depends` because the desktop
entry plays the library mpd serves.

## Flags
- `RUSTFLAGS="-C target-feature=-crt-static"` — the tree-wide switch that links the program
  dynamically against musl. `CARGO_HOME` points cargo at a directory inside the build tree, and
  `CARGO_NET_OFFLINE=true` with `--frozen --offline` keeps every crate coming from the vendor
  bundle.
- `VERGEN_GIT_DESCRIBE` — set to `v<version>`. rmpc's `build.rs` asks git, through the vergen
  crate, for the `git describe` string that `rmpc version` and `rmpc debug-info` print and the log
  records at start, and a release tarball has no git history. vergen takes a value already in the environment; without one it puts
  the placeholder `VERGEN_IDEMPOTENT_OUTPUT` into the binary. `VERGEN_GIT_SHA` is set the same way,
  but `build.rs` asks vergen for the describe string alone, so nothing reads it.

## Notes
The vendor bundle, `rmpc-vendor-<version>.tar.xz`, is `cargo vendor` run by `ports/fetch`
(`vendoring = rust`) and held to its own `sha256 =` line; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

The manual page and the bash, fish and zsh completions are not in the tarball: `build.rs` generates
them into `target/man` and `target/completions` during the build, and `build.sh` installs them from
there.

The desktop entry is KDOS's own. It sets `Terminal=true` and `X-KDOS-Term=kdos-term`, which asks
for [kdos-term](../../../../docs/kdos/04-programs/kdos-term.md) whatever the session's terminal is,
because the album art is drawn in the terminal's cell grid; see
[Terminal entries](../../../../docs/kdos/04-programs/kdos-appbox.md#terminal-entries). rmpc's
visualiser pane runs `cava`, which is not a port, so that pane draws nothing (see
[Features whose library has no port](../../../../docs/kdos/06-reference/known-gaps.md#features-whose-library-has-no-port)).
