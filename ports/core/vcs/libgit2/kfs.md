## About

libgit2 is Git's core — repositories, objects, references, the index, fetch and push — as a C
library with a stable API, written separately from Git itself. KDOS builds it in `30_foundation`
for the Rust toolchain: `cargo`, which the `rust` port builds in `31_compilers`, and `cargo-c`
link it through the `git2` crate, as do Rust programs such as `delta`, `eza` and `bat`. Each of
them would otherwise compile the copy of libgit2 that `libgit2-sys` carries. How a Rust port is
made to take the system library is in
[Writing ports](../../../../docs/kdos/05-developer/writing-ports.md#rust). `kicad` and
`horizon-eda` link it directly.

Only the shared library, its headers, `libgit2.pc` and the CMake package files are installed;
the `git2` command-line program upstream builds by default is left out.

## Flags

- `-DBUILD_SHARED_LIBS=ON` — build `libgit2.so` rather than a static archive; upstream's default,
  stated. `libgit2-sys` and `kicad` link this shared library.
- `-DBUILD_TESTS=OFF`, `-DBUILD_CLI=OFF`, `-DBUILD_EXAMPLES=OFF` — skip the test suite, the `git2`
  command-line program and the example programs. The first two are on by default.
- `-DUSE_HTTPS=OpenSSL` — use OpenSSL for HTTPS, from the `openssl` port. The default picks a TLS
  library by itself.
- `-DUSE_SHA1=CollisionDetection` — compute SHA-1 with libgit2's bundled SHA-1DC code, which
  detects the known collision attack on SHA-1 and refuses an object built with it, rather than
  with OpenSSL's plain SHA-1. This is upstream's default, stated.
- `-DUSE_SHA256=HTTPS` — take SHA-256 from the HTTPS library, here OpenSSL. Also the default.
- `-DUSE_SSH=libssh2` — support `ssh://` remotes through the `libssh2` library. Off by default;
  the other provider, `exec`, runs the `ssh` program instead.
- `-DUSE_HTTP_PARSER=builtin` — parse HTTP with the copy of llhttp in `deps/llhttp`. The other
  choices are a system `http-parser` or `llhttp`, and neither is a port.
- `-DREGEX_BACKEND=pcre2` — use the system PCRE2 for regular expressions. Left unset, libgit2
  takes the C library's `regcomp_l`, which musl does not have, then a system PCRE (version 1,
  not a port), and otherwise compiles the PCRE2 copy it carries in `deps/pcre2`.
- `-DUSE_BUNDLED_ZLIB=OFF` — link the system zlib rather than the copy in `deps/zlib`.
- `-DUSE_GSSAPI=OFF` — no Kerberos (SPNEGO) authentication for HTTP. GSSAPI comes from `krb5`,
  which is built in `41_system`, well after this phase.
- `-DUSE_NTLMCLIENT=ON` — NTLM authentication for HTTP servers and proxies, through the
  ntlmclient code in `deps/ntlmclient`, which uses the HTTPS library's cryptography. On by default
  whenever HTTPS is.
- `-DUSE_THREADS=ON`, `-DUSE_NSEC=ON` — thread support, and nanosecond file times in the index
  so a change within one second is still noticed. Both are defaults, stated.
- `-DENABLE_REPRODUCIBLE_BUILDS=ON` — run `ar` and `ranlib` in deterministic mode (`D`), so a
  static archive carries no timestamps or owner ids. This build makes no archive that reaches the
  package, so it changes nothing installed.
