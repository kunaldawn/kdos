## About

ublksrv is the user-space server for [ublk]{.idx}, the kernel's interface for block devices
implemented by a user-space program: the kernel creates `/dev/ublkbN` and hands each I/O request to
the server through io_uring, and the server answers it from a file, a network service or memory. The
port installs `libublksrv`, for writing such servers, and the `ublk` tool with its targets: `ublk
add -t loop` backs a device with a file, `-t nbd` with an NBD server, `-t nfs` with a file on an NFS
share, and `-t null` with nothing, for testing. It needs the kernel's `ublk_drv` module, which the
[linux](#port-linux) port builds (`CONFIG_BLK_DEV_UBLK=m`). The upstream repository is
`ublk-org/ublksrv`; the port and its package are named `ubdsrv`.

## Flags

- `autoreconf -fi` — The GitHub tag archive carries `configure.ac` and `Makefile.am` but no
  generated `configure`, so the build system is generated first; this is why
  [autoconf](#port-autoconf), [automake](#port-automake) and [libtool](#port-libtool) are in
  `depends`.
- `-fpermissive` — Added to `CXXFLAGS` for the C++ targets. It makes g++ report some kinds of
  nonconforming code as warnings instead of errors.
- `--sbindir=/usr/sbin` — Where `ublk` and its per-target programs (`ublk.loop`, `ublk.nbd`,
  `ublk.nfs` and the rest) are installed; it is the default for this prefix, stated.
- `--with-libnfs` — Builds the NFS target, `ublk.nfs`, against [libnfs](#port-libnfs). configure
  builds it only when libnfs offers its version 2 interface with multithreading
  (`nfs_mt_service_thread_start`). When that test fails it drops the target silently, with this
  switch as much as with the default, `check`, so the switch does not make a missing libnfs an
  error.
- `--without-gnutls` — Builds the NBD target without TLS, so `ublk.nbd` cannot connect to an NBD
  server that requires it. By default configure links GnuTLS when pkg-config finds it.
- `--without-libiscsi` — Does not build the iSCSI target, `ublk.iscsi`. By default configure builds
  it when pkg-config finds libiscsi.
- `noinst_PROGRAMS=` — Empties the list of the two demonstration programs, `demo_null` and
  `demo_event`, which are built and never installed, so neither `make` builds them.

## Patches

- `musl-prctl.patch` — Removes `#include <linux/prctl.h>` from `lib/ublksrv.c`, which already includes `<sys/prctl.h>`. musl's `<sys/prctl.h>` defines the same `PR_*` constants and `struct prctl_mm_map` itself, so including both is a redefinition error.
