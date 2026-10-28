## About

LIRC decodes infrared remote controls. `lircd` reads a receiver through one of its driver plugins
(the kernel's rc-core devices through `devinput`, or USB, serial, FTDI, HID and audio-input
receivers through their own), turns the pulses into button names using the remote's `lircd.conf`,
and serves them on the socket `/run/lirc/lircd`, which Kodi, VLC and every other `liblirc_client`
program read. `irexec` runs commands on buttons, `irw` prints what arrives, and `lircd-uinput`
turns buttons into ordinary input events. The port also installs the Python bindings. How the
service starts is in
[Services](../../../../docs/kdos/02-user-guide/administration.md#services).

## Flags

- `--runstatedir=/run` — The run-state directory compiled into LIRC's path defaults (`VARRUNDIR`),
  under which the socket and the PID file go. `/var/run` is only a link to `/run` here, so the
  `/var/run/lirc` paths in the shipped `lirc_options.conf` reach the same place.
- `--enable-devinput`, `--enable-uinput` — Builds the `devinput` driver, which reads `/dev/input`
  event devices and is the driver `lirc_options.conf` names, and `lircd-uinput`. configure's
  defaults guess both from what the build host has under `/dev`, so each is named.
- `--with-lockdir=/run/lock` — The directory for the old-style device lock files. configure's
  default is `/var/lock/lockdev` or `/var/lock`, again chosen from the build host; `/var/lock` is
  only a link to `/run/lock`.
- `--with-systemdsystemunitdir=no` — Installs no systemd units. KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--without-x` — Builds neither `irxevent` nor `xmode2`, which drive an X server.

## Patches

- `0008-fix-event-time-on-32bit.patch` — From Alpine's package. In the `devinput` plugin's trace message, reads an input event's time through the kernel's `input_event_sec` and `input_event_usec` names instead of `event.time.tv_sec` and `tv_usec`, which are not members of `struct input_event` where the kernel headers use its y2038-safe layout (a 32-bit target with a 64-bit `time_t`).
- `0010-lirc-add-include-for-major.patch` — From Alpine's package. Includes `<sys/sysmacros.h>` in `lib/drv_enum.c` and `plugins/default.c`, which call `major()` and `minor()`; musl does not provide them through `<sys/types.h>`.
- `musl-gcc14.patch` — From Alpine's package. Removes the `-1` initialiser of the `mplay` plugin's `pthread_t` listener field, and, in `mplayfamily_deinit()`, the test of the field against `-1` and its reset to `-1`, so the function cancels and joins the listener thread directly. `pthread_t` is a pointer type on musl, and GCC 14 and later reject an integer initialiser for it.

## Notes

The receiver plugins for libusb, FTDI, ALSA and PortAudio devices are built only when configure
finds their libraries, with no switch for any of them, so every library they need is in `depends`:
the libusb plugins use the libusb-0.1 API of [libusb-compat](#port-libusb-compat), and the FTDI
one needs [libftdi](#port-libftdi) and [libusb](#port-libusb) together.
`build.sh` checks for `atilibusb.so`, one of the libusb plugins, after the install, so a build in
which the probe failed stops there.

The Python package is `lirc` in `site-packages`, with the `_client` extension inside it.
`lirc/client.py` imports `lirc.config`, the module that holds the configured paths
(`SYSCONFDIR`, `VARRUNDIR`), but `Makefile.am` installs the generated `config.py` only into the
copy of the package source under `/usr/share/lirc/python-pkg`, one directory above its package;
`build.sh` installs it into `site-packages/lirc` as well. It then imports `lirc.client` and
`lirc.config` from the package with `LD_LIBRARY_PATH` naming the package's own `liblirc_client`,
so a package in which the bindings cannot load stops the build, and checks that `_client.so` is
still there afterwards, since importing `lirc` runs `paths.py`.

configure installs systemd units whatever `--with-systemdsystemunitdir` says when the build host's
`/proc/version` names Ubuntu, and inside the chroot that file belongs to the host's kernel; the
recipe deletes `lib/systemd` and `usr/lib/systemd` from the package. The install also creates
`/var/run/lirc`, a path through the `/var/run` link that a package cannot own; it is deleted, and
`64_lircd.sh` creates `/run/lirc` before it starts the daemon, since `lircd` does not create its
socket directory itself.

`/etc/init.d/64_lircd.sh` starts `lircd --nodaemon` under the supervisor only when a receiver is
present: a `/dev/lirc*` node or an rc-core device under `/sys/class/rc`. With neither, there is
nothing to decode and it skips.

`lirc-setup` is deleted. It is a GTK 3 configuration wizard that downloads its list of remotes, and
each remote's configuration, from the lirc-remotes project on SourceForge, so offline it has no
remote to offer; a remote's `lircd.conf` is placed by hand under `/etc/lirc/lircd.conf.d` instead.
