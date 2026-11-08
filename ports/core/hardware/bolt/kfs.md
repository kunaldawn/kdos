## About

bolt manages Thunderbolt devices: `boltd` authorises a device the kernel has enumerated,
by its stored policy, and `boltctl` lists, enrolls and authorises devices by hand. Where the
firmware's Thunderbolt security level is `user` or `secure`, the shipping default on most business
laptops, a Thunderbolt 3 or 4 dock is enumerated and then never authorised without it, and the dock's ports stay
dead. An enrolled device is authorised only while `boltd` runs.

## Flags

- `--localstatedir=/var` — Keep the device database, which holds the enrolled devices and their
  keys, at `/var/lib/boltd`. meson derives the state directory from the prefix, and the path is
  compiled into the daemon (`BOLT_DBDIR`) and created at install, so a prefix other than `/usr`
  with no `--localstatedir` would compile `boltd` to keep its keys somewhere else.
- `-Dman=true` — Build the manual pages, `boltctl(1)` and `boltd(8)`, and fail without the tool
  that makes them. At the default, `auto`, they are built only when `a2x` is found, so they would
  be dropped silently whenever [asciidoc](#port-asciidoc) is not installed first; it is in
  `depends` for this.
- `-Dinstall-tests=false` — Install none of the test programs, which is also upstream's default.
  They are compiled in any case, since
  bolt has no option to skip building them; the ones that need umockdev are not built, because
  that dependency is optional and umockdev is not a port.
- `-Dprivileged-group=wheel` — The group named in the polkit rule bolt installs, which grants it
  enroll, authorize and manage. `wheel` is upstream's default and a group in `/etc/group`. KDOS's
  `/etc/polkit-1/rules.d/50-kdos.rules` is read first and answers the same three actions with an
  administrator's password for anybody at the machine, so bolt's grant applies to none of them and
  letting a device onto the PCIe bus always asks (see
  [What `50-kdos.rules` changes](../../../../docs/kdos/03-architecture/security-model.md#what-50-kdosrules-changes)).

## Notes

No switch turns systemd off, because none is needed. bolt's `systemd` meson option is marked
`DEPRECATED` and read nowhere; the build installs a unit file only when
`dependency('systemd', required: false)` finds `systemd.pc`, which the tree does not have, and in
that case it creates the device database directory at install instead. Nothing links sd-bus
either: `boltd` uses GDBus, and its readiness notifications are its own write to
`$NOTIFY_SOCKET`, which is unset here, so they do nothing.

`boltd` is installed in `/usr/libexec`, the meson default, and the same path is written into the
D-Bus service file's `Exec=`.

**Starting boltd.** D-Bus activation is the only way `boltd` starts. Upstream's `90-bolt.rules`
starts it through `SYSTEMD_WANTS`, which eudev ignores. `build.sh` adds two pokes for the two
moments a controller can appear, each a D-Bus `Ping` to `org.freedesktop.bolt`, which activates
`boltd` when it is not running and is answered by it when it is, so neither can start a second
daemon:

- `91-kdos-bolt.rules`, a udev rule that sends the ping on every `add` in the `thunderbolt`
  subsystem once the bus is up;
- `/etc/init.d/43_boltd.sh`, which sends it at boot when `/sys/bus/thunderbolt/devices` is not
  empty. The coldplug replays the `add` events before the system bus has started, so the rule's
  `dbus-send` has nothing to talk to then.

The udev rule cannot start the daemon itself: udevd kills whatever a rule runs once the event is
handled. The init script's `stop` does nothing, since the bus started the daemon, and its `status`
reports whether the bus name has an owner.
