## About

PCRE2 is the Perl-compatible regular expression library. In KDOS it sits under GNU `grep` (for
`grep -P`), `less`, `glib` and so everything built on GLib, git and libgit2, zsh, VTE, ripgrep, and
both Qt bases, whose `QRegularExpression` is PCRE2. It installs one library per code-unit width
(`libpcre2-8`, `libpcre2-16` and `libpcre2-32`), the POSIX wrapper `libpcre2-posix`, and the
`pcre2grep` and `pcre2test` programs.

## Flags

- `export CFLAGS="${CFLAGS/-O2/-O3}"` — Compile the library at `-O3` instead of the tree's `-O2`,
  keeping the rest of the tree's flags; the checks a recipe's flags go through are in
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).
- `--enable-unicode` — Build Unicode support: UTF-8, UTF-16 and UTF-32 matching and the Unicode
  property tables. It is upstream's default, so naming it restates the default.
- `--enable-jit` — Build the just-in-time compiler, which turns a pattern into machine code for
  faster matching. It is off by default. A program asks for it by calling `pcre2_jit_compile()` on a
  pattern; a pattern not compiled that way is matched by the interpreter.
- `--enable-pcre2-16` — Also build `libpcre2-16`, for UTF-16 strings. Qt 6's `FEATURE_system_pcre2`
  looks for PCRE2's 16-bit library only, because a `QString` is UTF-16.
- `--enable-pcre2-32` — Also build `libpcre2-32`, for UTF-32 strings; `brltty` and rizin's
  `librz_util` link it.
- `--enable-pcre2grep-libz` — Link `pcre2grep` with [zlib](#port-zlib), so it searches `.gz` files
  directly.
- `--enable-pcre2grep-libbz2` — Link `pcre2grep` with [bzip2](#port-bzip2), so it searches `.bz2`
  files directly.
- `--enable-pcre2test-libreadline` — Link `pcre2test` with [readline](#port-readline) for line
  editing in its interactive mode.
