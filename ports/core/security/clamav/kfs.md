## About

ClamAV is an open-source anti-virus engine. The package holds the scanning library `libclamav`,
`clamscan` for scanning on demand, the `clamd` daemon with its client `clamdscan`, `clamonacc` for
scanning files as they are opened, `freshclam` to download the signature databases, and `sigtool`,
`clambc`, `clamdtop` and `clamconf`. Part of `libclamav` is written in Rust; its crates come
vendored in upstream's own release tarball, under `.cargo/vendor`.

The package ships no signature database. `clamscan` and `clamd` refuse to scan without one, and
`freshclam`, run once as root on a machine with a network, fetches it into `/var/lib/clamav`.

## Flags

- `export CARGO_HOME="$SRC_ROOT/.cargo"`, `export RUSTFLAGS="-C target-feature=-crt-static"`,
  `export CARGO_NET_OFFLINE=true` — The tree's [rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust)
  build shape, set in the environment because cargo is run by ClamAV's CMake rather than by the
  recipe. The `RUSTFLAGS` line makes the Rust part link against musl dynamically, as the C part
  does. ClamAV's `FindRust` passes `--offline` to cargo whenever `.cargo/vendor` exists, so the
  crates are taken from the tarball.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the programs and libraries with no `RPATH`; their
  libraries are in `/usr/lib`.
- `-DAPP_CONFIG_DIRECTORY=/etc/clamav` — Where `clamd.conf`, `freshclam.conf` and the rest are read
  from. Upstream's default is `etc` under the prefix, which would be `/usr/etc`. The certificates
  `freshclam` checks database signatures against follow it, into `/etc/clamav/certs`.
- `-DDATABASE_DIRECTORY=/var/lib/clamav` — Where the signature databases live. Upstream's default
  is `share/clamav` under the prefix, `/usr/share/clamav`; the databases are files `freshclam`
  downloads and replaces at run time, not files of the package.
- `-DBYTECODE_RUNTIME=interpreter` — Run bytecode signatures in ClamAV's own interpreter. The
  alternative, the LLVM JIT, accepts only LLVM 8 to 13, and the tree's [llvm](#port-llvm) is
  newer.
- `-DENABLE_APP=ON`, `-DENABLE_CLAMONACC=ON` — Build the programs, and on Linux `clamonacc`, as
  upstream does by default.
- `-DENABLE_MILTER=OFF` — Build no `clamav-milter`. It needs sendmail's `libmilter`, which is not
  a port.
- `-DENABLE_SYSTEMD=OFF` — Install no systemd unit files; KDOS has no systemd (see
  [No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-DENABLE_UNRAR=ON` — Build and install `libclamunrar`, so ClamAV can look inside RAR archives.
  Upstream's default.
- `-DENABLE_EXTERNAL_MSPACK=ON` — Link the [libmspack](#port-libmspack) port for CAB and CHM
  archives instead of ClamAV's own copy, `libclammspack`.
- `-DENABLE_JSON_SHARED=ON` — Link [json-c](#port-json-c) as a shared library; upstream's default.
- `-DHAVE_SYSTEM_LFS_FTS=ON` — Answer in advance the configure test for a working `fts(3)` with
  large-file support. musl has no `fts`, and the test links no `libfts`, so it fails even with
  musl-fts installed, and `clamonacc` would build its own copy, `misc/fts.c`; set, `clamonacc` uses [musl-fts](#port-musl-fts) instead, which
  `link-fts.patch` links.
- `-DENABLE_MAN_PAGES=ON` — Install the manual pages; upstream's default on Linux.
- `-DENABLE_DOXYGEN=OFF`, `-DENABLE_EXAMPLES=OFF`, `-DENABLE_TESTS=OFF` — No API documentation, no
  example programs, no unit tests.
- `-DENABLE_STATIC_LIB=OFF`, `-DENABLE_SHARED_LIB=ON` — Build `libclamav` and `libfreshclam` as
  shared libraries only; both are upstream's defaults.

## Patches

- `link-fts.patch` — Alpine's patch: adds `fts` to the libraries `clamonacc` links. With `HAVE_SYSTEM_LFS_FTS` set, `clamonacc` calls `fts_open()` and the rest from the system, and on musl those are in `libfts`, not the C library.
- `openssl4.patch` — Upstream's removal of `cl_ASN1_GetTimeT()` from `libclamav/clamav.h`, `libclamav/crypto.c` and the library's symbol map. The function reads the fields of `ASN1_TIME` directly, which OpenSSL 4 makes opaque, and nothing in ClamAV calls it.

## Notes

`build.sh` deletes `clamsubmit` and its manual page: the program does nothing but upload a sample
to Cisco's servers.

Upstream installs only `.sample` configuration files, which are never read, and `freshclam`
refuses to start without a configuration. `build.sh` therefore writes a three-line
`/etc/clamav/freshclam.conf`: the database directory, the `clamav` account as `DatabaseOwner`,
and `database.clamav.net` as the mirror.

`postinstall.sh` creates the `clamav` group and system account (home `/var/lib/clamav`, no login
shell) unless they exist, and gives it the database directory. `freshclam` run as root switches to
that account and exits if it is missing. The script writes under `PKG_ROOT`, so it also works when
`kpkgadd` installs into a root that is not the running one.
