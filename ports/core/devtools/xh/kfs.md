## About

xh is a command-line HTTP client in the style of HTTPie: `xh POST example.org name=value` builds a
JSON body from its arguments, and the response is printed with its headers and a highlighted,
indented body. Invoked as `xhs` it assumes `https://` when the URL names no scheme, so the package
installs `xhs` as a link to `xh`, with a matching manual page link. It is a Rust program built from
a vendored crate bundle, against the system's OpenSSL and oniguruma.

## Flags

- `--no-default-features` — Turns off upstream's default feature set: `rustls`, `online-tests` and
  `network-interface`. `rustls` would compile its TLS stack, and with it AWS-LC, into the binary,
  where no package database sees its version. `online-tests` only gates tests that reach the
  network. `network-interface` serves `--interface` on systems without `SO_BINDTODEVICE`; on Linux
  xh binds through the socket option and does not use it.
- `--features native-tls,http-message-signatures` — `native-tls` makes reqwest speak TLS through the
  system [openssl](#port-openssl), which is why `openssl` and `pkgconf` are in `depends`.
  `http-message-signatures` adds xh's support for signing requests (RFC 9421) through the
  `httpsig-hyper` crate, a feature upstream leaves off by default. The TLS rule for every Rust port
  is in
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).

## Notes

The two `readelf -d` lines after `cargo build` stop the build unless the binary NEEDs `libssl.so`
and `libonig.so.5`. The second holds because the tree's environment sets `RUSTONIG_SYSTEM_LIBONIG=1`
for every Rust build from `40_lang` on, which makes syntect's `onig_sys` link
[oniguruma](#port-oniguruma) instead of compiling its own copy. Either check failing means a copy of
the library was compiled into `xh`.
