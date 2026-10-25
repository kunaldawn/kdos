## About

atuin keeps shell history in a SQLite database and replaces `Ctrl+R` with a full-screen search over
it. The system `bash.bashrc` runs `atuin init bash --disable-up-arrow`, so `Ctrl+R` is atuin's
search and the Up arrow stays readline's (see
[Shell environment](../../../../docs/kdos/06-reference/configuration.md#shell-environment)). The
package is the client alone, built without sync, so the history stays on the machine.

## Flags

- `--exclude=vendor/axoasset` — Leave the bundle's copy of the `axoasset` crate out when unpacking.
  It is atuin's own path crate, copied without a `.cargo-checksum.json`, and cargo refuses a
  directory source that holds a crate without one. The build takes `axoasset` from the source
  tree's `vendor/axoasset`, which atuin's `Cargo.toml` names under `[patch.crates-io]`.
- `--config .kdos-vendor/.cargo/config.toml` — Point cargo at the bundle's configuration, which
  replaces crates.io with the bundle's `vendor/` directory. The bundle is unpacked into
  `.kdos-vendor` rather than the top of the tree because the tree already has a `vendor/` of its
  own (see Notes), and cargo reads the relative `directory = "vendor"` against the directory that
  holds `.cargo`, which is `.kdos-vendor`.
- `--package atuin` — Build the `atuin` binary crate of the workspace and no other member.
- `--no-default-features` — Turn off every default feature of the `atuin` crate: `client`, `sync`,
  `clipboard`, `check-update`, `daemon`, `ai` and `pty-proxy`.
- `--features client,clipboard,pty-proxy` — Turn three of them back on: `client` is the local
  SQLite store, the search interface and the shell hooks; `clipboard` copies a history entry out of
  the search interface through the `arboard` crate; `pty-proxy` runs the shell inside a local
  pseudo-terminal so that the search can be drawn inline. `sync`, `check-update`, `daemon` and `ai`
  stay off; in the `atuin` crate, they are the features that gate its network code.

## Notes

atuin's source tree has a `vendor/` directory of its own, holding `axoasset` as a
`[patch.crates-io]` path crate and a copy of `bash-preexec`. That `axoasset` carries no
`.cargo-checksum.json`, and a cargo directory source laid over the same `vendor/` refuses the whole
registry because of it. The bundle therefore goes beside the tree, in `.kdos-vendor`, and is named
with `--config` (see
[Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).

The two `readelf` lines after `cargo build` stop the build unless the binary needs
`libsqlite3.so.0` and `libzstd.so.1`. The tree's environment tells the `libsqlite3-sys` and
`zstd-sys` crates to link the system libraries through pkg-config, and a crate that compiled its
bundled copy instead would leave a private SQLite or zstd inside the binary, out of reach of those
ports' updates (see [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).

Feature selection stops at the `atuin` crate. The `atuin-client` library beneath it is still
compiled with its own default features, among them `sync`, because other workspace crates take it
with defaults on and cargo unions the features of one crate; that code is linked and never called
by the commands built here.
