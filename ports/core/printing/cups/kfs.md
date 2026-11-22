## About
CUPS is the print system: the `cupsd` scheduler that holds the queues, `libcups` that every
printing application links, the `lp`, `lpstat` and `lpadmin` commands, and the filters and
backends that carry a job to a printer. On KDOS it prints to driverless (IPP Everywhere) printers
it discovers through [avahi](#port-avahi), and Plasma's print-manager manages its queues; see
[Printing](../../../../docs/kdos/02-user-guide/administration.md#printing).
`/etc/init.d/80_cups.sh` runs `cupsd -f` under supervision.

## Flags
- `-Wl,-z,stack-size=8388608`, added to `LDFLAGS` — ask for an 8 MiB stack in each program's
  `PT_GNU_STACK` header, the size a thread gets by default on glibc. musl gives each new thread
  128 KiB unless that header asks for more, and libcups starts every thread with default
  attributes (`_cupsThreadCreate`). `cupsd` adds a driverless printer on such a thread: it
  connects to the printer, resolves its `.local` name through Avahi
  (`local-names-through-avahi.patch`) and builds the PPD from the printer's attributes, all on
  that one stack. CUPS's `Makedefs` puts `LDFLAGS` on its programs (`cupsd`, the commands,
  backends and filters) and not on `libcups.so`, so the size reaches no program that only links
  the library. `readelf -lW /usr/sbin/cupsd` shows it as the `GNU_STACK` entry's `0x800000`.
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
- `--with-system-groups="lpadmin root"` — Members of `lpadmin`, and root, may manage printers:
  `/etc/cups/cups-files.conf` says `SystemGroup lpadmin root`. Without the switch configure picks
  the groups it finds in the build root's `/etc/group`, so the result would follow the build
  machine. The `kdos` account is in `lpadmin`, which is the authority print-manager acts with.
  `root` is there because `cupsd` grants an administrative request only to a member of a system
  group, and root's own group is `root`: with `lpadmin` alone, `sudo lpinfo -v` and `sudo lpadmin`
  are answered `Forbidden`. `lpadmin` stays first, the group that owns the local root
  certificate.
- `--enable-acl` — Link [acl](#port-acl), so that `cupsd` gives every system group after the
  first, `root` here, read access to its local root certificate with a POSIX ACL.
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
  so `cupsd` runs all the time while the service is enabled.

## Patches
`local-names-through-avahi.patch` gives `httpAddrGetList()`, the lookup every libcups client
connects through, a second way to resolve a host name in the mDNS `.local` domain. musl has no
NSS, so no nss-mdns module answers such a name for `getaddrinfo()`, and every printer that
DNS-SD discovers is named by one: without the patch `cupsd` refuses to add a discovered
driverless printer with `Unable to connect to <host>.local:8631: Name does not resolve`, although
Avahi resolved the service. When `getaddrinfo()` fails for a name ending in `.local` (or
`.local.`), the patch asks the Avahi daemon through `avahi-client`, which libcups already links
under `--with-dnssd=avahi`, waits at most two seconds, and returns the address with the port asked
for; an IPv6 link-local answer carries the interface it came on as its scope. `cupsd`, the `ipp`
backend, `lpadmin`, `lpstat`, cups-browsed and Plasma's printer settings then all resolve `.local`
names. A lookup that `getaddrinfo()` answers, and every name outside `.local`, is unchanged. With
the Avahi daemon down the second lookup fails at once and the original error stands. The patch
is to be offered upstream.

## Notes
`no-idle-exit.patch` removes the `IdleExitTimeout` line from the `cupsd.conf` that `make install`
writes. The scheduler knows that directive only when it is built for on-demand starts, which
`--with-ondemand=no` turns off, so with the line in place every start of `cupsd` logs `Unknown
directive IdleExitTimeout` in `/var/log/cups/error_log`. No configure switch leaves it out:
`--with-idle-exit-timeout=no` still writes it, as `IdleExitTimeout 0`. It is a fault upstream
could take.

`build.sh` stages the install with `make BUILDROOT=$PKG install`. `BUILDROOT` is the prefix every
install path in CUPS's `Makedefs` is built on; it defaults to `$(DSTROOT)$(DESTDIR)`, so setting it
directly stages the same tree.

After the install `build.sh` removes `/tmp`, `/run` and `/var/run` from the package (tmpfs at run
time, and refused by the lint), sets `/var/cache` and `/var/spool` to `0755`, and makes every
staged file writable by its owner.

The configuration files under `/etc/cups` and the backends under `/usr/lib/cups/backend` keep the
restricted modes CUPS gives them; both paths are on the lint's allowlist of files that need not be
world-readable. The installer enables the service by default; a person who turns printing off
in the installer gets `/etc/service.disabled/cups`, and `service enable cups` turns it back on.
