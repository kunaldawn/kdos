## About

[newsboat]{.idx} is an RSS and Atom feed reader for the terminal, with `podboat`, its podcast
downloader. It is C++ over a Rust library, `libnewsboat`, which its makefile builds with cargo and
links in statically. The recipe writes a terminal desktop entry for it, named Feeds.

## Flags

- `CARGO_NET_OFFLINE=true` — Keep cargo off the network. The makefile runs cargo itself and passes
  no offline flag, so the switch reaches it through the environment.
- `-lintl` — Link libintl. The makefile adds `-liconv -lintl` only when `uname -s` is neither Linux
  nor GNU, which suits glibc, where gettext is in the C library. musl has no gettext of its own, so
  without the flag the link fails on `libintl_gettext`. It goes in `LDFLAGS` because that is last on
  the link line, after the objects, and `NEWSBOAT_LIBS` and `PODBOAT_LIBS` are plain assignments the
  makefile would overwrite. `-liconv` is left out: musl has iconv in libc.
- `-Wno-error=unused-function` — Keep one warning from being an error. The makefile compiles with
  `-Werror` and appends `CXXFLAGS` after its own warnings. json-c's `linkhash.h` defines its helpers
  `static` with `_LH_INLINE`, which is `inline` only when `__STDC_VERSION__` names C99 or later. A
  C++ compiler defines no `__STDC_VERSION__`, so in C++ every helper a file leaves unused is a
  `-Wunused-function` warning.
- `-g0` — Cancel the `-ggdb` in the makefile's own `BARE_CXXFLAGS`; no package ships debug
  information.
- `install-completions` — Install the zsh and fish completions from `contrib/completions`; the
  target copies files and builds nothing.

## Notes

The make goals are named rather than running `make` and `make install`. The default target's `doc`
builds the HTML manual and FAQ as well as the two manual pages, and `install-docs` installs that
HTML, the changelog and `contrib/`. `build.sh` asks only for `doc/newsboat.1` and `doc/podboat.1`,
which asciidoctor builds with upstream's `doc/man.rb` converter, and installs them by hand.

The Rust crates come from the vendor bundle, `newsboat-vendor-<version>.tar.xz`, pinned with its own
`sha256 =` line and unpacked at the top of the tree, where the makefile runs cargo (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).
The recipe sets no `RUSTFLAGS`: cargo builds `libnewsboat` as a static archive, and the final link
is the C++ compiler's, not rustc's.
