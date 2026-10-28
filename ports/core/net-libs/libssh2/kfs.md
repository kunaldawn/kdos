## About

libssh2 is a client-side implementation of the SSH-2 protocol as a C library: sessions,
authentication, channels, SCP and SFTP. It has no server side and no command-line programs of its
own; the `ssh` command on KDOS is OpenSSH's.

[curl](#port-curl), in the same phase, `30_foundation`, links it for its `scp://` and `sftp://`
URLs. [libgit2](#port-libgit2) uses it for SSH remotes, and through libgit2 and their own
`-sys` crates [rust](#port-rust) and [cargo-c](#port-cargo-c) do too; [vlc](#port-vlc),
[mc](#port-mc), [nmap](#port-nmap), [libvirt](#port-libvirt), [imhex](#port-imhex) and
[gst-plugins-bad](#port-gst-plugins-bad) depend on it as well. The `libssh2-sys` crate asks `pkg-config` for
this library only when `LIBSSH2_SYS_USE_PKG_CONFIG=1` is set, which both of those `build.sh` files
export, and compiles a copy of its own in silence when it finds none; see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).

## Flags

- `--with-crypto=openssl` — the cryptography library libssh2 is built on. The default, `auto`,
  tries OpenSSL, libgcrypt, mbedTLS, WinCNG and wolfSSL in that order and takes the first it finds;
  [libgcrypt](#port-libgcrypt) and [mbedtls](#port-mbedtls) are ports too, in `41_system`. Named,
  OpenSSL is the only backend configure looks for, and a missing OpenSSL stops it with
  `Required dependencies are missing.` instead of falling back to another. It is the same library
  [curl](#port-curl), which links this one, uses for TLS.
- `--with-libz` — compression of the SSH transport through [zlib](#port-zlib). The default, `auto`,
  turns compression off with a notice when zlib is not found; naming it makes a missing zlib an
  error.
- `--disable-examples-build` — do not compile the example programs under `example/`, which upstream
  builds by default and never installs.
- `--disable-rpath` — a switch of gnulib's library-search macros, which configure uses to find
  OpenSSL and zlib: with it, they hard-code no directory they found a library in as a run path in
  `libssh2.so`. Every library it links is in `/usr/lib`, the loader's default path, and the package
  lint warns on such an entry (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
