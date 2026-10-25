## About
[Jujutsu]{.idx}, the `jj` command, is a version control system whose repositories are ordinary git
repositories, so git keeps working on the same tree. Every operation on the repository is recorded
in an operation log, and `jj op restore` puts the whole repository back to any recorded moment.
For fetch and push it runs the `git` command, so
[git](#port-git) is in `depends`.

It is a Rust program built with cargo from its vendor bundle, the second `sha256 =` line, with its
default features, `git` and `watchman` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `OPENSSL_NO_VENDOR=1` — tell the `openssl-sys` crate never to compile its own OpenSSL and to
  link the system's. The present bundle holds no `openssl-sys`, so it changes nothing in this
  build; a version whose dependencies bring the crate in links the [openssl](#port-openssl) port
  rather than a private copy (see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
- `--bin jj` — build only the `jj` program. Run at the top of the workspace without it, cargo
  builds every member package, the `lib/gen-protos` generator among them. The `fake-*` test
  programs the `cli` package declares need the `test-fakes` feature and are skipped either way.

## Notes
The manual pages and the shell completions are written by the program just built:
`jj util install-man-pages` and `jj util completion bash`, `zsh` and `fish`. A `jj` that cannot run
in the build root fails the build there, after `cargo build` has succeeded.
