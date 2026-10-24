## About
nfs-utils is the user-space half of NFS: `mount.nfs`, the helper `mount -t nfs` runs, for the
client, and `exportfs`, `rpc.nfsd` and the daemons that serve exports from the kernel's NFS server,
plus the ID mapping and Kerberos pieces both sides use. Where samba is how KDOS talks to Windows
machines and phones, NFS is how it shares files with another Unix machine with POSIX ownership,
symbolic links and byte-range locks intact. The server is NFSv4 only. The recipe writes two boot
services, `63_gssd` and `72_nfsd`, each of which skips itself until it has something to do (a
Kerberos keytab, a share in `/etc/exports`); see
[Services](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#services).
`mount.nfs` is setuid, so an ordinary user can mount a share listed in `fstab`.

## Flags
- `-D_LARGEFILE64_SOURCE` — Exposes musl's `stat64` and the other `*64` names. The internal
  `rpcgen` this build compiles calls `stat64()` on `struct stat64`, which musl declares only under
  this macro.
- `-include libgen.h` — Includes `<libgen.h>` ahead of every source file. `nfsdctl.c` calls
  `basename()` without it, and musl declares `basename` only there; upstream builds with
  `-Werror=implicit-function-declaration`, so the call stops the build.
- `-include stddef.h` — Includes `<stddef.h>` ahead of every source file, so `size_t`, `NULL` and
  `offsetof` are declared in every file, whichever headers it includes.
- `-Wno-error=format` — Turns format-string errors back into warnings. Upstream's configure adds
  `-Werror=format=2` to every compile.
- `--enable-nfsv4` — NFSv4 support, with `rpc.idmapd` and `nfsidmap` for mapping names to IDs.
  Upstream's default.
- `--enable-nfsv4server` — Build `nfsv4.exportd`, the export daemon of an NFSv4-only server, which
  `72_nfsd` supervises in place of `rpc.mountd`. Off by upstream's default.
- `--enable-gss` — Build `rpc.gssd`, the client side of RPCSEC_GSS, which a `sec=krb5` mount needs;
  linked against [krb5](#port-krb5). Upstream's default.
- `--disable-svcgss` — Do not build `rpc.svcgssd`, the server side of RPCSEC_GSS. Off is upstream's
  default.
- `--enable-ipv6` — IPv6 transport for the client and server. Upstream's default.
- `--enable-caps` — Link [libcap](#port-libcap) into `rpc.statd`, `nfsdcld`, `nfsdcltrack` and
  `nfsref`, which use it to drop capabilities they do not need. Named, the switch makes missing
  libcap headers a configure error.
- `--enable-uuid` — Use libblkid, from [util-linux](#port-util-linux), to find an exported
  filesystem's UUID for the export cache, and skip the version check that would otherwise turn
  the feature off for an old libblkid.
- `--enable-ldap` — Build the `umich_ldap` ID-mapping plugin for libnfsidmap, against
  [openldap](#port-openldap). Named, the switch makes a missing LDAP library a configure error
  instead of a silent omission.
- `--without-systemd` — Install no systemd units; KDOS has no systemd. Off is also upstream's
  default.
- `--with-statedir=/var/lib/nfs` — The state directory, where `rpc_pipefs` is mounted and
  `nfsdcld` keeps its client records. This is upstream's default, and both boot services write
  the same path.
- `--with-rpcgen=internal` — Generate the RPC stubs with the `rpcgen` in nfs-utils' own `tools/`
  directory instead of a system one. Without it, configure looks for `rpcgen` in `/usr/local/bin`,
  `/usr/bin` and `/bin` and stops when it finds none, and the recipe does not depend on
  [rpcsvc-proto](#port-rpcsvc-proto), which provides it.
- `--disable-sbin-override` — Install `mount.nfs` and the other mount helpers into `--sbindir`
  (`/usr/sbin`) rather than forcing them into `/sbin`. On KDOS `/sbin` is a symbolic link to
  `usr/sbin`, and a package carrying a real `/sbin` would collide with it; the
  `test ! -e "$PKG/sbin"` line checks that none was written.

## Notes
`make install` also installs the internal `rpcgen` and its manual page. Both are deleted, because
[rpcsvc-proto](#port-rpcsvc-proto) owns `/usr/bin/rpcgen` and kpkg refuses a path another package
owns. `nfs.systemd.7`, a manual page about the systemd units, is deleted with them.

`72_nfsd` starts the server with `rpc.nfsd -N 3 -V 4`: NFSv3 is off, so no portmapper,
`rpc.mountd`, `rpc.statd` or lock daemon is started, although the build installs them. It loads
`nfsd`, mounts the `nfsd` and `rpc_pipefs` filesystems, supervises `nfsdcld` and `nfsv4.exportd`
in the foreground (`-F`, `-f`) so the supervisor can restart them, and runs `exportfs -r` before
starting the kernel threads. Stopping it stops the threads first, so no request arrives for an
export that is being withdrawn.

`nfsv4.exportd` links only with optimisation on: `exportd.c` defines a C99 `inline` function with
no external definition, which is an undefined reference at `-O0`.

`autoreconf -f -i` regenerates the release's `configure` and `Makefile.in` with the tree's
autotools before configuring.
