## About
[Inlyne]{.idx} is a Markdown and HTML viewer that draws the document itself on the GPU through
wgpu, with no browser engine underneath, and reloads it when the file changes on disk. It is a Rust
program built with cargo from its vendor bundle, the second `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). The desktop entry
`build.sh` writes opens a file with `inlyne view`.

## Flags
- `-p inlyne` — build the `inlyne` package alone. The source is a cargo workspace whose other
  member, `xtask`, is a helper that regenerates the shell completions (`cargo xtask gen`); the
  completions it writes are already in the release under `completions/`, and `build.sh` installs
  those as they are.

## Notes
Syntax highlighting goes through `syntect`, which binds Oniguruma by way of the `onig_sys` crate.
From `40_lang` on the tree exports `RUSTONIG_SYSTEM_LIBONIG`, which makes that crate link the
[oniguruma](#port-oniguruma) port instead of compiling the copy it carries (see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)). The `readelf` line after
`cargo build` checks that the binary needs `libonig.so.5`, so a build that falls back to the
bundled copy stops instead of shipping it.

`inlyne.default.toml`, upstream's example configuration, is installed as documentation under
`/usr/share/doc/inlyne`. Inlyne reads its settings from `~/.config/inlyne/inlyne.toml`, which a
user copies it to; nothing is installed under `/etc`.
