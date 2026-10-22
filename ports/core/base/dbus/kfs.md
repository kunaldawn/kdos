## About

D-Bus is the message bus most Linux system services and desktop programs talk to each other over.
This port installs `dbus-daemon`, the reference [libdbus]{.idx} client library and the command-line
tools (`dbus-send`, `dbus-monitor` and the rest). KDOS runs two kinds of bus with it. The system
bus is started at boot by `/etc/init.d/40_dbus.sh` and carries NetworkManager, polkit,
ModemManager and the other root services. Each user's session runs one session bus
at a fixed address in `$XDG_RUNTIME_DIR`, which boxed applications reach at the same path; see
[The session message bus](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#the-session-message-bus).

The build differs from a systemd distribution's in what it leaves out: no systemd integration, no
X11 autolaunch, no AppArmor or SELinux mediation and no audit logging. Activation of root services
still goes through `dbus-daemon-launch-helper`, the setuid helper the port's `postinstall.sh`
repairs.

## Flags

- `--libexecdir=/usr/lib/$name` — Puts `dbus-daemon-launch-helper` in `/usr/lib/dbus/`, the path
  the system bus configuration names in `<servicehelper>`, and the one `postinstall.sh` and
  `kdos doctor` check.
- `-D x11_autolaunch=disabled` — Builds no X11 autolaunch: with it, libdbus links libX11 and a
  client with no session bus address can start one through the X display. KDOS has no X server on
  the login path, and the session starts its bus itself.
- `-D systemd=disabled` — Builds no systemd support, which upstream describes as `at_console`
  through systemd. KDOS has no systemd; see
  [No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd).
- `-D launchd=disabled`, `-D kqueue=disabled` — macOS launchd autolaunch and the BSD kqueue event
  loop, which do not apply on Linux. Each defaults to `auto`.
- `-D epoll=enabled`, `-D inotify=enabled` — Use epoll for the daemon's main loop and inotify to
  notice changed configuration and service files. `enabled` makes configure fail if either is
  missing, where `auto` would quietly build without it.
- `-D apparmor=disabled`, `-D selinux=disabled`, `-D libaudit=disabled` — Build no AppArmor or
  SELinux mediation of bus messages and no audit logging. None of libapparmor, libselinux or
  libaudit is a port.
- `-D qt_help=disabled`, `-D ducktype_docs=disabled`, `-D doxygen_docs=disabled` — Build none of
  the Qt help file, the Ducktype (Mallard) documentation or the Doxygen API reference.
- `-D modular_tests=disabled` — Builds none of the GLib-based regression tests.
- `-D xml_docs=enabled` — Builds the manual pages and HTML documentation from DocBook with
  `xsltproc`, from [libxslt](#port-libxslt), [docbook-xsl](#port-docbook-xsl) and
  [docbook-xml](#port-docbook-xml). `enabled` stops configure when the DocBook stylesheets cannot
  be found, where `auto` would build without the manual pages.
- `-D system_pid_file=/run/dbus/pid`, `-D system_socket=/run/dbus/system_bus_socket` — The
  system bus's pid file and listening socket, written into `system.conf` and compiled into
  libdbus as the default system bus address. They are the paths meson derives from
  `--prefix=/usr` anyway.

## Notes

`rm -rf $PKG/run` removes the empty `/run/dbus` directory the install creates. `/run` is a tmpfs
created afresh at every boot, so nothing in a package can live there; `40_dbus.sh` makes
`/run/dbus` before it starts the daemon.

Some clients have the old address `unix:path=/var/run/dbus/system_bus_socket` compiled in, basu's
`sd_bus_open_system()` among them. The daemon listens under `/run`, and they reach it because
`/var/run` is a link to `/run`, made by `script/phases/10_bootstrap/000_file_system.sh`. A root
where `/var/run` is a real directory leaves every such client unable to find the system bus.

The machine id the bus needs, `/var/lib/dbus/machine-id`, is not in the package: an image that
carried one would give every machine the same id. `40_dbus.sh` creates it on first boot with
`dbus-uuidgen --ensure` and links `/etc/machine-id` to it.

kpkg installs every file `root:root`, so the launch helper loses the `messagebus` group upstream's
install gives it. `postinstall.sh` sets it back, owner `root:messagebus` and mode `4110`, or the
bus cannot start any activated root service; see
[setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries).
