## About
[Node.js]{.idx} is the JavaScript runtime built on V8, with npm. In KDOS it is a build tool more
than a program anyone runs: the browsers ([chromium](#port-chromium),
[firefox-esr](#port-firefox-esr), [librewolf](#port-librewolf)), [thunderbird](#port-thunderbird)
and [qt6-qtwebengine](#port-qt6-qtwebengine) run JavaScript at build time, and
[yarn](#port-yarn) and [cncjs](#port-cncjs) run on it.

The KDOS build links Node against the system's libraries wherever its configure offers a switch for
it: c-ares, zlib, libuv, OpenSSL, zstd, libffi, brotli, nghttp2, SQLite and ICU each come from
their own port, and the copies under `deps/` in the tarball are not compiled. A security fix to
one of those ports reaches Node when the library is rebuilt, without rebuilding Node.

## Flags
- `CFLAGS="${CFLAGS/-O2/-O3}"` — Raises the tree's `-O2` to `-O3` in both `CFLAGS` and
  `CXXFLAGS`. Node's Release configuration (`common.gypi`) compiles at `-O3`, and the exported
  flags come after it on every compile line, so without this the tree's `-O2` would win and Node
  would build below its own Release level.
- `--shared-cares` — Links the system c-ares, Node's asynchronous DNS resolver, instead of the
  copy in `deps/cares`.
- `--shared-zlib` — Links the system zlib instead of `deps/zlib`.
- `--shared-libuv` — Links the system libuv, the event loop, instead of `deps/uv`.
- `--shared-openssl` — Links the system OpenSSL instead of the copy in `deps/openssl`, so `node`
  carries no TLS library of its own.
- `--openssl-use-def-ca-store` — Trusts OpenSSL's own certificate store, `/etc/ssl`, instead of
  the copy of Mozilla's CA list compiled into Node. A certificate the system trusts is then trusted
  by Node too.
- `--shared-zstd` — Links the system zstd instead of `deps/zstd`.
- `--shared-ffi` — Links the system libffi, which Node's foreign-function interface uses, instead
  of `deps/libffi`.
- `--shared-brotli` — Links the system brotli instead of `deps/brotli`.
- `--shared-nghttp2` — Links the system nghttp2, Node's HTTP/2 library, instead of
  `deps/nghttp2`.
- `--shared-sqlite` — Links `node:sqlite` against the system SQLite instead of `deps/sqlite`.
  `node:sqlite` calls `sqlite3session_*` and `sqlite3_column_table_name`, so the
  [sqlite](#port-sqlite) port must keep its session extension (`--session`) and column metadata
  (`SQLITE_ENABLE_COLUMN_METADATA`); without them this build fails to link.
- `--v8-enable-temporal-support` — Builds V8's `Temporal` date and time API. Without the switch,
  configure turns Temporal off with only a warning whenever `--with-intl=system-icu` is given, and
  also when it finds no `cargo` or `rustc`; with it, a missing Rust toolchain stops configure. The
  Rust crates Temporal is written in are vendored in the tarball under `deps/crates`, and
  [rust](#port-rust) is in `depends` for this.
- `--with-intl=system-icu` — Takes `Intl`, Unicode and time-zone support from the system ICU
  ([icu](#port-icu)) instead of the ICU subset in `deps/icu-small`. This is what the Temporal
  patch works around: V8 reaches Temporal's time-zone data through an ICU header that a
  system ICU does not install.

## Notes
The two lines before `configure` belong together. The patch makes V8 compile Temporal's time-zone
rules into the binary from the `zoneinfo64` crate's `zoneinfo64.res`, and the `install` line then
replaces that file with the one the icu port compiles from its own source, under
`/usr/share/icu/<version>/`. The rules Temporal uses are therefore the system ICU's tz release, but
set when Node is built: an icu update with new time-zone rules reaches `Intl` at once
and reaches `Temporal` only when this port is rebuilt. The path is read from
`pkg-config --modversion icu-uc`, so it follows the installed icu without an edit here.

npm installs its manual pages inside its own module, under `/usr/lib/node_modules/npm/man/`, where
`man` does not look. The loop at the end copies each section into `/usr/share/man`, so
`man npm-install` works.
