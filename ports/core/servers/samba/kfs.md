## About

Samba implements SMB, the file- and printer-sharing protocol of Windows. In KDOS it is built as a
file and print server, `smbd`, with the client tools and libraries: `smbclient`, `libsmbclient`,
which [gvfs](#port-gvfs), [kodi](#port-kodi) and [vlc](#port-vlc) use to open `smb://` addresses,
and `ntlm_auth`, which [libsoup3](#port-libsoup3) tries for NTLM sign-on. A Windows laptop, a phone
or a network printer often speaks SMB and nothing else. The Active Directory domain controller,
domain membership and winbind are not built.

The package carries a service script, `83_samba`, that runs `smbd` under the supervisor once a
non-empty `/etc/samba/smb.conf` exists and skips itself before then. It creates `/var/log/samba`
and `/var/cache/samba` before each start, because `smbd` never makes its log directory and writes
nothing but the supervisor's log without it. It passes `supervise --stop-grace 15`, so `smbd`
has time to end its per-client children and close their open files before the supervisor's
`SIGKILL`; five seconds is the default. Other machines reach the server after
the firewall's `smb` name is opened (see
[Services](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#services)).

## Flags

- `export XSLTPROC="xsltproc --maxdepth 10000"` — The manual pages are built with `xsltproc`, and
  `smb.conf.5` nests the DocBook templates deeper than libxslt's default limit of 3000, so the
  task would stop on "potential infinite template recursion" with every binary already built.
  waf takes `XSLTPROC` from the environment and splits it as a command line, so the limit is
  raised here rather than in the source.
- `--enable-fhs` — Lay files out by the Filesystem Hierarchy Standard. Samba refuses `--prefix=/usr`
  without it ("Don't install directly under /usr or /usr/local without using the FHS option"),
  and its own layout would put everything under `/usr/private`, `/usr/var` and the like.
- `--with-piddir=/run/samba`, `--with-privatedir=/var/lib/samba/private`,
  `--with-logfilebase=/var/log/samba` — Where Samba keeps its PID files, its private databases
  (the password database and secrets) and its logs.
- `--bundled-libraries='!talloc,!tdb,!tevent,ALL'` — Take talloc, tdb and tevent from their ports
  ([talloc](#port-talloc), [tdb](#port-tdb), [tevent](#port-tevent)) and build every other library
  Samba carries (ldb among them, which is not a port) from its own copy. Named outright, a missing
  system library is a configure error and not a quiet switch to the bundled copy.
- `--without-ad-dc` — Build no Active Directory domain controller. What remains is the file server, the client tools and the libraries.
- `--without-ldb-lmdb` — Build ldb without its LMDB backend, which exists for the domain
  controller's database. Samba does not drop it on its own: without the switch it stops with "ldb
  build (unless --without-ldb-lmdb) requires lmdb 0.9.16 or later".
- `--without-ads`, `--without-ldap` — No Active Directory membership, and no LDAP, which in Samba
  serves the domain-member and `ldapsam` account backends; [openldap](#port-openldap) being in the
  build root must not switch them on.
- `--without-winbind` — Build no `winbindd`. `ntlm_auth` still installs, but it signs on only from
  winbindd's cached credentials, so libsoup's attempt with it fails and libsoup answers NTLM
  itself (see [Networking](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#networking)).
- `--without-pam` — Build without PAM: no `pam_winbind` module (winbind is not built either) and
  no PAM account and session checks in `smbd`, whose `obey pam restrictions` setting then has
  nothing to call. SMB clients are still checked against Samba's own password database, which
  `smbpasswd` writes.
- `--without-systemd` — No systemd notification or units; KDOS has no systemd (see
  [No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--with-acl-support` — POSIX ACL support, through [acl](#port-acl). Without it a Windows client
  cannot change a file's permissions, and its security tab is read-only.
- `--without-quotas` — No disk quota support.
- `--with-libunwind` — Samba's crash backtraces. musl has no `execinfo.h` (`backtrace()` is a glibc
  extension), so without libunwind the fault handler cannot print one, and configure stops rather
  than choose. The probe wants `libunwind-generic.pc` and `libunwind.h`, which come from
  [libunwind-nongnu](#port-libunwind-nongnu).
- `--without-lttng`, `--without-fam`, `--without-dmapi`, `--without-winexe`,
  `--disable-glusterfs`, `--disable-cephfs` — LTTng tracing, FAM file notification, DMAPI,
  `winexe` (which needs a MinGW cross compiler), GlusterFS and CephFS. None of these is a port, and
  each switch keeps Samba from deciding by what happens to be installed.
- `--without-kernel-keyring` — The kernel keyring stores Kerberos credentials, and there is no realm
  to join.
- `--with-regedit` — Build `samba-regedit`, the curses registry editor, on [ncurses](#port-ncurses).
- `--with-shared-modules='vfs_io_uring,!vfs_snapper'` — Name `vfs_io_uring`, which Samba builds
  only when pkg-config finds liburing, so a missing [liburing](#port-liburing) fails the build
  instead of leaving the module out silently. Strike `vfs_snapper`: it talks to snapperd over
  D-Bus, snapper is not a port, and left in the default list it makes D-Bus a configure-time
  requirement.
- `--disable-spotlight` — No Spotlight search; its one real backend queries an Elasticsearch
  server.
- `--enable-cups` — A `[printers]` share can offer Windows clients every queue the local
  [cups](#port-cups) has. waf answers a missing libcups by building without printing, so the
  `config.h` check after configure fails the build instead.
- `--disable-iprint` — No Novell iPrint, which uses the same library.
- `--enable-avahi` — Advertise shares over mDNS with [avahi](#port-avahi), so a file manager on
  another machine finds them without a typed address.
- `--disable-rpath`, `--disable-rpath-install` — The first leaves the `RPATH` out of the binaries
  in the build tree, the second leaves the `RPATH` to the library directory, `/usr/lib`, out of the
  installed files. `--disable-rpath-private-install` is not given, so installed files keep their
  `RPATH` to Samba's private libraries in `/usr/lib/samba`.
- `--nopyc`, `--nopyo` — Install no compiled Python bytecode for Samba's Python modules.

## Patches

- `musl-netlink.patch` — In `source3/wscript`, the two compile tests for Linux netlink and rtnetlink include `<sys/socket.h>` instead of `<bits/sockaddr.h>`. `<bits/sockaddr.h>` is a glibc header that musl does not have, so on musl both tests fail and Samba builds without `HAVE_LINUX_RTNETLINK_H`, which `source3/lib/addrchange.c` needs to follow changes to the machine's addresses over netlink.

## Notes

After `configure`, `build.sh` greps `bin/default/include/config.h` for `HAVE_CUPS` and for
`HAVE_LINUX_RTNETLINK_H`, and stops when either is missing. The second is the proof that the patch
took effect.

`rm -rf "$PKG/run" "$PKG/var/run" "$PKG/var/lock"` drops the runtime directories the install
creates; `/run` is a fresh tmpfs at every boot. The `pkg-config --exists` line fails the build
unless the `ndr_standard`, `samba-util` and `smbclient` pkg-config files were installed, the ones
the client library's consumers build against.

Samba's build system, waf, is Python run at build time, which is why [python3](#port-python3) is in
the `depends` of a C program, and it builds inside the source tree.
