## About
CUPS is the print system: the `cupsd` scheduler that holds the queues, `libcups` that every
printing application links, the `lp`, `lpstat` and `lpadmin` commands, and the filters and
backends that carry a job to a printer. On KDOS it prints to driverless (IPP Everywhere) printers
it discovers through [avahi](#port-avahi), and `kdos-print` manages its queues; see
[Printing](../../../../docs/kdos/02-user-guide/administration.md#printing).
`/etc/init.d/80_cups.sh` runs `cupsd -f` under supervision.

## Flags
- `--with-menudir=/usr/share/applications` — Install the CUPS web interface's desktop entry in
  the standard place, with `--with-icondir=/usr/share/icons` for its icons.
- `--with-logdir=/var/log/cups` — Where `cupsd` writes its logs; with `--with-docdir=/usr/share/cups`
  (the web interface's pages) and `--with-rundir=/run/cups` (its run-time state) the paths are
  stated rather than derived from the prefix.
- `--with-domainsocket=/run/cups/cups.sock` — The local socket clients reach the scheduler on.
  The init script creates `/run/cups` before it starts `cupsd`.
- `--with-cupsd-file-perm=0755` — The mode `make install` gives `cupsd`. Upstream's default is
  `0700`, which the [package lint](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)
  refuses, because a file that is not readable by everyone must be a credential on its
  allowlist.
- `--with-cups-user=lp` — Run filters and backends as `lp`, in group `lp`
  (`--with-cups-group=lp`); both are in `fs/etc/passwd` and `fs/etc/group`.
- `--with-system-groups=lpadmin` — Members of `lpadmin` may manage printers. Without the switch
  configure picks the groups it finds in the build root's `/etc/group`, so the result would follow
  the build machine. The `kdos` account is in `lpadmin`, which is the authority `kdos-print` acts
  with.
- `--enable-acl` — Link [acl](#port-acl), so that `cupsd` can give every system group after the
  first read access to its local root certificate with a POSIX ACL. With `lpadmin` the only
  system group, the certificate's group ownership covers it and no ACL is written.
- `--enable-dbus` — Send job and printer events on the system D-Bus, with
  `--with-dbusdir=/usr/share/dbus-1` naming where the bus configuration goes.
- `--enable-pam` — Authenticate web interface and `lpadmin` requests through PAM; `build.sh`
  writes `/etc/pam.d/cups`, which checks the password with `pam_unix`.
- `--enable-libpaper` — Take the default paper size from [libpaper](#port-libpaper).
- `--with-dnssd=avahi` — Discover and share printers with DNS-SD through Avahi's API.
- `--enable-libusb` — The USB backend through [libusb](#port-libusb).
- `--enable-raw-printing` — Accept jobs sent as raw data, passed to the printer without filtering.
  It is the default.
- `--enable-relro` — Link CUPS's programs and libraries with `-Wl,-z,relro,-z,now`.
- `--with-tls=gnutls` — Encrypt IPP connections with [gnutls](#port-gnutls).
- `--with-optim` — The optimisation flags CUPS compiles both its C and its C++ with. It is given
  the tree's `CFLAGS` with `-std=gnu11` removed, because that standard is valid for C only and
  `g++` warns about it on every C++ file; see
  [C flags handed to the C++ compiler](../../../../docs/kdos/05-developer/build-troubleshooting.md#c-flags-handed-to-the-c-compiler).
- `--without-rcdir` — Install no System V init scripts; the init script is KDOS's own.
- `--without-systemd` — Install no systemd units, and with `--with-ondemand=no` build no
  socket-activation support. KDOS has
  [no systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd),
  so `cupsd` runs all the time once enabled.

## Notes
`build.sh` stages the install with `make BUILDROOT=$PKG install`. `BUILDROOT` is the prefix every
install path in CUPS's `Makedefs` is built on; it defaults to `$(DSTROOT)$(DESTDIR)`, so setting it
directly stages the same tree.

After the install `build.sh` removes `/tmp`, `/run` and `/var/run` from the package (tmpfs at run
time, and refused by the lint), sets `/var/cache` and `/var/spool` to `0755`, and makes every
staged file writable by its owner.

The configuration files under `/etc/cups` and the backends under `/usr/lib/cups/backend` keep the
restricted modes CUPS gives them; both paths are on the lint's allowlist of files that need not be
world-readable. The installer leaves the service disabled; it is enabled with
`service enable cups`.
