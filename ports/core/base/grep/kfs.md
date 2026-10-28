## About
GNU grep searches text for lines matching a pattern: basic and extended regular expressions,
fixed strings, and Perl-compatible expressions with `grep -P`. `egrep` and `fgrep` are installed as
the small scripts upstream ships. On KDOS every `grep` is this one, which matters to build systems:
configure scripts and `build.sh` files lean on GNU behaviour such as `\|` in a basic pattern
([The base userland](../../../../docs/kdos/05-developer/build-troubleshooting.md#the-base-userland)).

`10_bootstrap` cross-compiles the same source without PCRE ([Grep](#03-bootstrap-066-grep)).
This port rebuilds it natively in the first run of `30_foundation`, linked to
[pcre2](#port-pcre2), and takes over the files the bootstrap installed.

## Flags
- `--disable-nls` — Install no message catalogues; messages are in English only.
- `--enable-perl-regexp` — Build `grep -P` against `libpcre2-8`. Named, configure stops with
  "PCRE support not available" when pcre2 is missing; left to its default, it would build
  without, and `grep -P` would answer only "Perl matching not supported in a --disable-perl-regexp
  build".
