## About
ripgrep, `rg`, searches a directory tree for a regular expression, line by line, skipping what
`.gitignore` and `.ignore` files exclude and hidden and binary files by default. The port builds it
with PCRE2 support, so `rg -P` takes Perl-compatible patterns with look-around and
backreferences, and generates its manual page and its bash, zsh and fish completions from the
built binary.

## Flags
- `--features 'pcre2'` — Build in the PCRE2 engine behind `-P`/`--pcre2`. ripgrep's default
  build has only its own regex engine.
- `PCRE2_SYS_STATIC=0` — Tell the `pcre2-sys` crate to link the system's `libpcre2-8` through
  pkg-config. Its build script otherwise assumes a static build on any musl target and compiles
  the PCRE2 copy it carries into the binary, where the [pcre2](#port-pcre2) port's updates never
  reach it.

## Notes
`rg` is built from a vendor bundle, `ripgrep-vendor-<version>.tar.xz`, which `ports/fetch` makes
with `cargo vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring). The installed binary
NEEDs `libpcre2-8.so.0`; one that does not has fallen back to the bundled copy.
