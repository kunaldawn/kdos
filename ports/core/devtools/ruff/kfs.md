## About
[Ruff]{.idx} is a Python linter and formatter written in Rust. It is installed the way upstream
publishes it, as a Python distribution built by [maturin](#port-python3-maturin) with
`bindings = "bin"`: the `ruff` executable goes to `/usr/bin`, and the small `ruff` Python
package beside it in `site-packages` finds and runs that executable, so `python3 -m ruff` works
too. [python3-lsp-ruff](#port-python3-lsp-ruff) reaches it through the module, and
[spyder](#port-spyder) depends on it.

## Flags
- `MATURIN_PEP517_ARGS="--frozen"` — Extra arguments for the `maturin` command that maturin's
  PEP 517 backend runs when pip calls it. `--frozen` is one of the cargo options maturin hands
  on to `cargo`: the build takes exactly what `Cargo.lock` names from the vendor bundle, and stops
  instead of resolving anything else.
- `CARGO_NET_OFFLINE=true` — Cargo's switch for never touching the network, set in the
  environment because pip, not `build.sh`, runs cargo.

## Notes
On x86-64 Linux `ruff` links tikv-jemallocator, compiled from the vendor bundle, rather than
using musl's `malloc`; the choice is a target-specific dependency in its `Cargo.toml`, not a
feature.

`test -x "$PKG/usr/bin/ruff"` stops the build if maturin produced a wheel without the
executable. The shell completions are written by running that binary, `ruff
generate-shell-completion <shell>`, after the install.
