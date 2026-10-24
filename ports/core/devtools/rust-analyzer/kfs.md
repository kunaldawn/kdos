## About
[rust-analyzer]{.idx} is the language server for Rust: an editor starts it and asks it for
completion, go-to-definition, type hints, diagnostics and refactorings over the Language Server
Protocol. Only the server binary is built (`-p rust-analyzer`); editors bring their own client.

It reads two things from the [rust](#port-rust) toolchain at run time, and that port installs
both: the standard library's source under `lib/rustlib/src`, without which it resolves nothing
in `std` or `core` and reports a sysroot error, and
`libexec/rust-analyzer-proc-macro-srv`, which expands derive and attribute macros.

## Flags
- `CARGO_PROFILE_RELEASE_DEBUG=0` — Build the release profile with no debug information. The
  workspace's own release profile already says `debug = 0`, and the phase exports the same value
  to every build; the line restates both.
- `CFG_RELEASE=$version` — The version the server reports, compiled in by `version.rs`;
  without it the server calls itself `0.0.0`. Its build script also treats an unset
  `CFG_RELEASE` as a developer's build from source and compiles in `POKE_RA_DEVS`, which makes
  the server show internal errors to the editor's user as pop-up messages.
- `-p rust-analyzer` — Build only the `rust-analyzer` package of the workspace, the server
  binary.
- `--features jemalloc` — Link the server against jemalloc, compiled from the
  `tikv-jemalloc-sys` crate in the vendor bundle, instead of musl's `malloc`.

## Notes
The port's version is the release tag's date, `YYYY.MM.DD`; the tag itself is spelled with
hyphens, which the recipe's `_tag` helper writes, and the source unpacks into
`rust-analyzer-YYYY-MM-DD`.
