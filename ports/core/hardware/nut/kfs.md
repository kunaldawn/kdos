## About

Network UPS Tools monitors uninterruptible power supplies, inverters and power distribution units,
and shuts the machine down cleanly before the battery runs out. Its drivers talk to the device,
`upsd` serves the readings, and `upsmon` watches them and starts the shutdown; `upsc` and
`nut-scanner` read and find devices. With `MODE` in `/etc/nut/nut.conf` the machine is a
stand-alone UPS host, a server for other machines, or a client of another machine's `upsd`. The
init script, the firewall name and the final power cut are described in
[Administration](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md) and
[Shutdown](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#shutdown).

## Flags

- `--sysconfdir=/etc/nut`, `--libexecdir=/usr/lib/nut`, `--datadir=/usr/share/nut`,
  `--with-drvpath=/usr/lib/nut` — Keep NUT's configuration in `/etc/nut`, its drivers and helpers
  in `/usr/lib/nut` and its data in `/usr/share/nut`. Upstream's default puts everything under
  `/usr/local/ups`.
- `--with-user=nut`, `--with-group=nut` — The account the drivers, `upsd` and the unprivileged
  half of `upsmon` drop to. `postinstall.sh` creates it.
- `--with-statepath=/run/nut`, `--with-altpidpath=/run/nut` — Keep the drivers' sockets and the
  PID files of the programs that do not run as root in `/run/nut`.
- `--with-udev-dir=/usr/lib/udev` — Install the USB rules that give the `nut` group the UPS
  devices. At the default, `auto`, `configure` installs them only when `/lib/udev` or `/etc/udev`
  exists on the build machine.
- `--with-serial`, `--with-usb`, `--with-modbus`, `--with-gpio`, `--with-linux_i2c`,
  `--with-snmp`, `--with-neon`, `--with-ipmi`, `--with-freeipmi`, `--with-powerman` — Build every
  family of drivers the tree can: serial, USB ([libusb](#port-libusb)), Modbus
  ([libmodbus](#port-libmodbus), for solar charge controllers and inverters), GPIO
  ([libgpiod](#port-libgpiod)), I²C ([i2c-tools](#port-i2c-tools)' `libi2c`), SNMP
  ([net-snmp](#port-net-snmp)), the XML/HTTP driver ([neon](#port-neon)), IPMI power supplies
  ([freeipmi](#port-freeipmi)) and power distribution units through a [powerman](#port-powerman)
  daemon. configure's default for most of these is to detect the library and leave the drivers
  out when it is missing.
- `--without-upower`, `--without-macosx_ups` — Build neither the driver that reads devices managed
  by UPower nor the macOS one.
- `--with-avahi`, `--with-libltdl`, `--with-nut-scanner` — Build `nut-scanner`, which loads its
  scanning back ends through `libltdl` and finds NUT servers on the network through
  [avahi](#port-avahi).
- `--with-ssl`, `--with-openssl`, `--without-nss` — Encrypt connections to `upsd` with
  [openssl](#port-openssl), not NSS.
- `--without-wrap` — No tcp-wrappers access control in `upsd`.
- `--without-cgi` — Build none of the CGI programs and HTML pages for a web server.
- `--without-nut_monitor`, `--without-pynut`, `--without-nutconf` — Install neither the Python
  NUT-Monitor application nor the PyNUT module, and build no `nutconf` tool.
- `--with-dev` — Install the client libraries `libupsclient` and `libnutclient` and their headers,
  for programs that talk to `upsd`. Off by default.
- `--with-doc=man=dist-auto` — Install the manual pages the release tarball carries, building them
  only when they are missing, and no HTML or PDF documentation.
- `--with-systemdsystemunitdir=no`, `--with-systemdsystempresetdir=no`,
  `--with-systemdshutdowndir=no`, `--with-systemdtmpfilesdir=no`, `--with-systemdsysusersdir=no`,
  `--without-libsystemd` — Install no systemd files and link no `libsystemd`. KDOS has no systemd
  ([No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--with-hotplug-dir=no`, `--with-devd-dir=no`, `--with-augeas-lenses-dir=no`,
  `--with-solaris-smf=no`, `--with-solaris-init=no`, `--with-solaris-pkg-svr4=no`,
  `--with-solaris-pkg-ips=no` — Install none of the integration files for the old Linux 2.4
  hotplug system, FreeBSD's `devd`, Augeas or Solaris.
- `--enable-cppunit=no` — Build no C++ unit tests.

## Notes

`/etc/nut/*.sample` are upstream's templates and are installed as they are. Nothing starts until
`/etc/nut/nut.conf` exists with a `MODE` of `standalone`, `netserver` or `netclient`; the init
script, `56_nut.sh`, reads that line itself and skips otherwise.

The init script supervises each program in the foreground (`-F`) under `ksvc`. `standalone` and
`netserver` start the drivers through `upsdrvctl`, then `upsd` and `upsmon`; `netclient` starts
`upsmon` alone, watching another machine's `upsd`. Each program started as root drops to the
`nut` account itself, except `upsmon`'s parent, which stays root to run the shutdown. The script
creates `/run/nut` owned by `nut`, because the drivers and `upsd`, running as `nut`, cannot create
it under `/run`, and it skips when the `nut` account is missing.

`postinstall.sh` creates the account under `PKG_ROOT`, the root the package is being installed
into, so an install into another tree (`--root`, or an A/B update) gives that tree the account.
It also puts `nut` in the `dialout` group, the group the tree's udev rules give serial adapters
and other plug-in devices. Both checks read
`etc/group` and `etc/passwd` under `PKG_ROOT` directly, so they test the tree being installed into
rather than the running system's account database.
