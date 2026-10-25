## About

libnbd is the client library for the Network Block Device protocol, with its command-line tools:
`nbdsh`, a Python shell over the library; `nbdcopy`, `nbdinfo` and `nbddump`; `nbdfuse`, which
shows an NBD export as a file through FUSE; and `nbdublk`, which attaches one as a block device
through the kernel's ublk driver. [libvirt](#port-libvirt) and [nbdkit](#port-nbdkit) link it.

## Flags

- `--enable-fuse` — Build `nbdfuse`, against fuse3. It is upstream's default.
- `--enable-python` — Build the Python binding, installed into python3's `site-packages`. `nbdsh`
  is written on it, so it is the switch that gives the package `nbdsh`.
- `--with-gnutls` — TLS through GnuTLS, for `nbds://` URIs and TLS-protected exports.
- `--with-libxml2` — URI parsing through libxml2, which `nbd://` and `nbds://` URIs need.
- `--enable-ublk` — Build `nbdublk`, against [ubdsrv](#port-ubdsrv)'s `libublksrv` and
  [liburing](#port-liburing). configure leaves `nbdublk` out with only a warning when `ublksrv` is
  missing, so `build.sh` checks that `nbdublk` was installed and fails if it was not.
- `--disable-ocaml`, `--disable-golang`, `--disable-rust` — The OCaml, Go and Rust bindings. No
  port here builds against them.
- `--with-bash-completions` — Install the bash completions, into the directory
  [bash-completion](#port-bash-completion)'s pkg-config file names; with the switch given,
  configure fails if that module is missing instead of installing none.

## Patches

- `musl-unistd.patch` — Adds `#include <unistd.h>` to `ublk/tgt.c`, which calls `gettid` and `getpid` without including the header that declares them. glibc's other headers bring the declarations in; musl's do not, and the implicit declaration is an error in current GCC.
