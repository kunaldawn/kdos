## About

switcheroo-control is a small system-bus service, `net.hadess.SwitcherooControl`, that tells a
launcher which GPUs a machine has, which of them is discrete, and which environment variables start
a program on each. KIO's "run on the discrete GPU" and `switcherooctl launch` read it. Its udev
rule tags each discrete GPU, running a small probe for amdgpu, nouveau and xe cards that asks the
kernel driver through [libdrm](#port-libdrm), and its hardware-database file names Intel's GPUs.
The probes are why the port is built in `42_graphics` rather than with the other hardware services
in `41_system`.

## Flags

- `-Dsystemdsystemunitdir=/usr/lib/systemd/system` — Where upstream installs its systemd unit. Left
  empty, meson asks `systemd.pc` for the directory and stops, since KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd));
  the unit is installed into the named directory and `build.sh` deletes it from the package.
- `-Dhwdbdir=/usr/lib/udev/hwdb.d`, `-Drulesdir=/usr/lib/udev/rules.d` — Where the
  hardware-database file (`30-pci-intel-gpu.hwdb`) and the rule (`30-discrete-gpu.rules`) go, named
  directly rather than read from `udev.pc`.
- `-Dlibdrm=enabled`, `-Dlibdrm_amdgpu=enabled`, `-Dlibdrm_nouveau=enabled`, `-Ddrm_xe=enabled` —
  The three probe programs, `switcheroo-control-check-discrete-amdgpu`, `-nouveau` and `-xe`. Each
  is `auto` upstream and is built only when its library or header is found; `enabled` makes each a
  requirement.
- `-Dgtk_doc=false`, `-Dtests=false` — No API documentation and no test run, upstream's defaults.

## Notes

`/etc/init.d/44_switcheroo-control.sh` starts the daemon under the supervisor only on a machine
with two or more GPUs: it counts the `card*` entries under `/sys/class/drm` that have a `device`,
skipping the connector entries, whose names contain a dash. With one GPU there is nothing to choose
between, and the service skips (see
[Services that skip themselves](../../../../docs/kdos/02-user-guide/administration.md#services-that-skip-themselves)).
