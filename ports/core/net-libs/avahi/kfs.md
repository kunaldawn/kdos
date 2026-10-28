## About
Avahi is an mDNS/DNS-SD implementation: `avahi-daemon` answers and sends multicast DNS on the
local network, and programs browse for and publish services through its D-Bus API with
`libavahi-client`. [CUPS](#port-cups) uses it to find driverless printers. Other ports link it
for service discovery too, among them [gvfs](#port-gvfs) and [geoclue](#port-geoclue) through
`libavahi-glib` and [mumble](#port-mumble) through the `libdns_sd` compatibility library. On
KDOS the daemon is started by `/etc/init.d/45_avahi.sh` once the system bus is up.

## Flags
- `--with-distro=none` — Install no distribution init script. configure detects the
  distribution from a release file such as `/etc/arch-release` and, finding none, stops with
  "Linux distribution autodetection failed"; `none` is the documented way past that. The
  daemon's start-up script is KDOS's own, under `fs/etc/init.d`.
- `--with-systemdsystemunitdir=no` — Install no systemd unit files. Without it configure asks
  pkg-config for systemd's unit directory. KDOS has
  [no systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd).
- `--with-xml=expat` — Parse `.service` files in `/etc/avahi/services` with expat, and fail
  configure if it is missing, instead of taking whichever of expat and bsdxml it finds first.
- `--with-avahi-user=avahi` — The account `avahi-daemon` drops to, with `--with-avahi-group`,
  `--with-autoipd-user` and `--with-autoipd-group` naming the rest: `avahi` and
  `avahi-autoipd`, the names upstream defaults to, stated so the build matches the two accounts
  `postinstall.sh` creates.
- `--with-avahi-priv-access-group=wheel` — The group whose members the daemon's D-Bus policy lets
  call every method, `SetHostName` among them; anyone else may browse, resolve and publish.
  Upstream's default is `netdev`, a Debian group KDOS does not have, so the policy would name a
  group that does not exist and the bus would log the unknown group at every start. `wheel` is the
  group of the users who administer the machine.
- `--enable-dbus` — Build the daemon's D-Bus interface and `libavahi-client`, which talks to it.
  CUPS, gvfs, geoclue and the `libdns_sd` compatibility library all reach the daemon through
  `libavahi-client`.
- `--enable-gdbm` — Build `avahi-browse`'s lookup of human-readable service type names in a GDBM
  database. configure stops if GDBM is missing.
- `--enable-glib` — Build `libavahi-glib`, the adapter that runs Avahi's callbacks on a GLib main
  loop, and with `--enable-gobject` `libavahi-gobject`, its GObject wrapper.
- `--disable-qt3` — Build none of the toolkit front ends: neither the Qt 3, Qt 4 nor Qt 5 main
  loop adapters (`--disable-qt4`, `--disable-qt5`) nor the GTK 2 and GTK 3 libraries and
  programs (`--disable-gtk`, `--disable-gtk3`). The GTK 3 one cannot be built at all, because
  GTK 3 depends on CUPS, which depends on Avahi.
- `--disable-mono` — Build no Mono (.NET) bindings, and with `--disable-monodoc` no
  documentation for them. The tree has no Mono.
- `--disable-python` — Install none of Avahi's Python programs (`avahi-discover`,
  `avahi-bookmarks`) and, with `--disable-pygobject`, nothing that needs PyGObject.
- `--disable-introspection` — Generate no GObject introspection data for `libavahi-gobject`.
- `--disable-libevent` — Build no `libavahi-libevent` main loop adapter; configure would
  otherwise require [libevent](#port-libevent) through pkg-config.
- `--disable-libsystemd` — Do not link libsystemd, through which the daemon would take its sockets
  from socket activation and report its state with `sd_notify`.
- `--disable-doxygen-doc` — Generate no API reference with Doxygen.
- `--enable-compat-libdns_sd` — Install `libdns_sd` and `dns_sd.h`, an implementation of Apple's
  Bonjour (mDNSResponder) client API on top of Avahi. Upstream leaves it off; mumble's zeroconf
  support finds it as `avahi-compat-libdns_sd`.
- `--disable-compat-howl` — Build no compatibility library for the HOWL API. It is upstream's
  default.
- `--disable-tests` — Build no test or example programs. It is upstream's default.
- `--enable-libdaemon` — Link `avahi-daemon` against [libdaemon](#port-libdaemon) for its PID file
  and signal handling. It is the default, and configure fails without libdaemon.

## Notes
`build.sh` runs `autoreconf -fi` first: the source is a tag archive from GitHub, not a release
tarball, and carries no generated `configure`.

`/run` and `/var/run` are removed from the staged tree, because both are tmpfs on a running
system and the [package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint)
refuses files there; the init script creates `/run/avahi-daemon` and gives it to `avahi` before
it starts the daemon. `ssh.service` and `sftp-ssh.service` are removed from
`/etc/avahi/services`, so a KDOS machine does not announce an SSH server on the local network.
`/var/lib/avahi-autoipd` is created in the package and given to `avahi-autoipd` by
`postinstall.sh`: it is that account's home and the directory the daemon chroots into.

With `--disable-python` the service type database `service-types.db` is not generated, because
upstream builds it with a Python script. `avahi-browse`, built with GDBM, then finds no database
and prints each service's type as the raw DNS-SD string.

`postinstall.sh` creates the two system accounts in the root being installed into. The daemon
exits at once if it cannot claim `org.freedesktop.Avahi` on the system bus, which is why
`45_avahi` starts after `40_dbus`.
