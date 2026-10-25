## About

delta is a pager for `git diff`, `git show`, `git log -p`, `diff` and `grep` output: it highlights
the syntax of the changed code, marks the changed words within a line, and can lay the two sides out
next to each other. Git uses it once `core.pager` and `interactive.diffFilter` name it. It is a Rust
program built from a vendored crate bundle, and it does its highlighting with [bat](#port-bat)'s
library and bat's compiled syntax and theme sets.

## Notes

- **The highlighting sets are bat's, not the bundle's.** bat embeds `syntaxes.bin`, `themes.bin` and
  `acknowledgements.bin` into any program that links it, and delta's copy of them sits in the
  vendored `bat` crate. Before cargo runs, `build.sh` copies the three files the [bat](#port-bat)
  port installed in `/usr/share/bat/assets` over those copies, so `delta` and `bat` highlight with
  the same sets. Cargo checks every vendored file against the crate's `.cargo-checksum.json`, so
  `build.sh` also rewrites the three entries there with the new files' sha256; the rest of the crate
  is still verified. Why the sets are built by the `bat` port, and how its asset bundle is made, is
  in [A bundle no tool
  writes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
- **The vendored bat must be the installed bat.** The sets are bincode dumps of syntect's types with
  no version tag, so a set from another bat may not load. `build.sh` reads the version of the
  vendored crate from `vendor/bat/Cargo.toml` and stops unless `bat --version` reports the same one.
  A bump of [bat](#port-bat) that delta's lock file does not follow stops this build, and the two
  have to move together.
- **libgit2 and oniguruma are the system's.** The tree's environment sets `LIBGIT2_NO_VENDOR=1` and
  `RUSTONIG_SYSTEM_LIBONIG=1` for every Rust build from `40_lang` on (see
  [rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust)),
  so the `-sys` crates link [libgit2](#port-libgit2) and [oniguruma](#port-oniguruma) instead of
  compiling their own copies. The two `readelf -d` lines after `cargo build` make that a condition
  of the build: the binary must NEED the libgit2 and oniguruma sonames they spell out in full, so a
  libgit2 release with a new soname stops this build until the check names it.
