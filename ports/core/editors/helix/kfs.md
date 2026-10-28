## About
[helix]{.idx}, the `hx` command, is a modal text editor for the terminal, in the line of vi and
Kakoune. It highlights and selects by syntax with tree-sitter grammars and talks to language
servers, both with no plugins to install. KDOS ships it on the host beside micro and neovim, and
the theme generators write `~/.config/helix/themes/kdos.toml` for it; helix uses that theme only
once `theme = "kdos"` is set in its `config.toml` (see
[Theming](../../../../docs/kdos/02-user-guide/theming.md)).

It is a Rust program built with cargo from its vendor bundle, the second `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). The port installs `hx`,
its runtime directory (grammars, queries, themes and the tutor) at `/usr/lib/helix/runtime`, and a
desktop entry with `Terminal=true`, since upstream ships none for a terminal program (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).

## Flags
- `HELIX_DEFAULT_RUNTIME=/usr/lib/helix/runtime` — read by `helix-loader` at compile time and built
  into `hx` as the last runtime directory it searches, after `~/.config/helix/runtime` and
  `$HELIX_RUNTIME`. Without it the one system directory `hx` searches is `runtime` beside its own
  binary, `/usr/bin/runtime`, and it starts with no grammars or themes unless a wrapper or a
  global variable points it elsewhere.
- `HELIX_DISABLE_AUTO_GRAMMAR_BUILD=1` — stops `helix-term`'s build script from fetching the
  grammars with git and compiling them during `cargo build`. The fetch needs the network, which
  the build does not have; the grammars are compiled in a separate step instead.
- `-C target-feature=-crt-static` — links `hx` dynamically against musl's `libc.so`. Rust's musl
  target links statically otherwise, and a static musl program cannot `dlopen`, which is how `hx`
  loads every grammar. `RUSTFLAGS` replaces the `rustflags` of a `.cargo/config.toml` rather than
  adding to them, so the flag has to be given here (see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
- `--grammar build` — compiles every grammar `languages.toml` names into a `.so` under
  `runtime/grammars`, with the C compiler the tree's `CC` names, from the grammar sources the
  release's source tarball carries under `runtime/grammars/sources`.

## Notes
`hx --grammar build` writes into the first runtime directory in its search order, and that is the
user's `~/.config/helix/runtime` unless `CARGO_MANIFEST_DIR` is set, in which case it is the
`runtime` directory beside the named crate's parent. The build sets it to `helix-term` in the
source tree so the grammars land in the tree's own `runtime/` before it is copied into `$PKG`. The
`test -f runtime/grammars/rust.so` after it fails the build at once if they went anywhere else,
rather than shipping a runtime with no grammars. Building the grammars also checks that a `git`
program is on the `PATH` before it compiles anything, even though it fetches nothing, which is why
[git](#port-git) is in `depends`. The grammar sources are removed from `$PKG` after the copy;
only the compiled grammars and the queries ship.

The vendor bundle carries a `.cargo/config.toml` of its own, which replaces the one in the source
tree when it is unpacked. Upstream's sets `--cfg tokio_unstable`, which only helix's tests use, so
dropping it changes nothing in the editor.
