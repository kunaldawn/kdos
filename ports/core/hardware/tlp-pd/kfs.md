## About

tlp-pd is [TLP](#port-tlp)'s power-profiles daemon: a Python program on GLib's D-Bus bindings
that owns `org.freedesktop.UPower.PowerProfiles` and `net.hadess.PowerProfiles` on the system bus,
the interface of power-profiles-daemon. PowerDevil and the battery applet's profile switcher talk
to it, and each switch runs `tlp power-saver`, `tlp balanced` or `tlp performance`, so TLP's own
settings stay the tuning under every profile. `tlpctl` is its command-line client.

It is a second build of TLP's source (see
[A second build of the same source](../../../../docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source)):
the same tarball under `tlp`'s file name and checksum, the same `make` variables, and only the
`install-pd` and `install-man-pd` targets. It is a port of its own because it needs
[python3-gobject](#port-python3-gobject), which is built in a later phase than `tlp`. Both recipes
carry `group = tlp`, and its `version` must equal `tlp`'s, because it runs the `tlp` beside it.

## Flags

- `TLP_WITH_ELOGIND=1` — With `TLP_WITH_SYSTEMD=0`, `TLP_ELOD=/usr/lib/elogind/system-sleep`,
  `TLP_SBIN=/usr/sbin`, `TLP_BIN=/usr/bin`, `TLP_TLIB=/usr/share/tlp`, `TLP_ULIB=/usr/lib/udev`,
  `TLP_CONFDIR=/etc/tlp.d` and `TLP_CONFDEF=/usr/share/tlp/defaults.conf`: `tlp`'s own `make`
  line, repeated so the generated scripts are byte for byte the ones `tlp` builds. `install-pd`
  puts `tlp-pd` in `TLP_SBIN` and `tlpctl` in `TLP_BIN`; `tlp-pd` runs `tlp` from `PATH` and
  reads the last profile `tlp` applied from its run directory, the one path generated into it.

## Notes

`install-pd` installs `tlp-pd.service` into `TLP_SYSD` whatever `TLP_WITH_SYSTEMD` says, and
D-Bus activation files for both bus names whose `Exec=` is `/bin/false` and whose
`SystemdService=` names that unit. `build.sh` removes `/usr/lib/systemd`, then the empty
`/usr/lib`, and `/usr/share/dbus-1/system-services`: the package lint refuses a systemd unit, and
an activation file that runs `/bin/false` would turn a call made before the daemon is up into a
failed activation. tlp-pd is started at boot instead, by `56_tlppd.sh` under `ksvc`, after
`55_tlp.sh` has applied the boot profile it reads back. The manual page `tlp-pd.service(8)`,
which documents the unit, is removed with it; `tlp-pd(8)` and `tlpctl(1)` ship.

The D-Bus policies under `/usr/share/dbus-1/system.d` let only root own the two names and let any
client call them; changing the profile is the polkit action in `tlp-pd.policy`.
