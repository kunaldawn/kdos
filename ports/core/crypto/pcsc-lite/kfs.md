## About
pcsc-lite is the PC/SC middleware between programs and smart card readers. `pcscd` owns the
readers, loads a driver for each from `/usr/lib/pcsc/drivers` (the USB CCID one is
[ccid](#port-ccid)), and serves `libpcsclite.so`, the PC/SC API that GnuPG's `scdaemon`,
[opensc](#port-opensc), [libfido2](#port-libfido2), [keepassxc](#port-keepassxc),
[openconnect](#port-openconnect), [freerdp](#port-freerdp)'s smart card redirection and the rest
call. On KDOS the service `47_pcscd` starts it at boot and keeps it running, and readers come and
go through udev; see [Services](../../../../docs/kdos/02-user-guide/administration.md#services).

## Flags
- `-Dlibsystemd=false` — No systemd socket activation or notification. The option has no default
  of its own, so meson takes it as on. KDOS has no systemd
  ([No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)); with it off the
  build still installs a socket and a service unit and a `sysusers.d` file under its prefix, and
  `build.sh` deletes all three.
- `-Dlibudev=true`, `-Dlibusb=false` — Learn of readers being plugged in and removed from udev,
  through [eudev](#port-eudev)'s `libudev`, which upstream prefers; libusb hotplug is the
  alternative, and the two cannot both be on.
- `-Dpolkit=true` — Ask polkit whether a client may use PC/SC and each card. pcscd then refuses
  any client that [polkit](#port-polkit) does not grant `org.debian.pcsc-lite.access_pcsc` and
  `org.debian.pcsc-lite.access_card`; KDOS grants both to `wheel` in
  `/etc/polkit-1/rules.d/50-kdos.rules`, so a user outside `wheel` reaches no reader. The two
  actions are in
  [What is granted instead](../../../../docs/kdos/03-architecture/security-model.md#what-is-granted-instead-50-kdosrules).
- `-Dusb=true`, `-Dserial=true` — Support USB readers, and serial readers configured by files in
  `/etc/reader.conf.d`.
- `-Dfilter_names=true` — Honour `PCSCLITE_FILTER_IGNORE_READER_NAMES` and
  `PCSCLITE_FILTER_EXTEND_READER_NAMES`, which hide readers by name or add text to their names.
  `47_pcscd` reads both from `/etc/default/pcscd` and exports them to the daemon.
- `-Dembedded=false` — Keep logging; the embedded build drops it to save memory and CPU.
- `-Dusbdropdir=/usr/lib/pcsc/drivers`, `-Dserialconfdir=/etc/reader.conf.d`,
  `-Dipcdir=/run/pcscd` — The driver directory, the serial-reader configuration directory and the
  directory of the daemon's socket, each upstream's default written out. `build.sh` creates the
  first two in the package, so both exist, owned by pcsc-lite, before any driver or reader
  configuration is installed.

## Notes
`build.sh` also deletes upstream's `/etc/default/pcscd`, whose comment describes `PCSCD_ARGS` as
read by the systemd unit. The base filesystem ships its own `/etc/default/pcscd` with the same keys,
which `47_pcscd` parses at every start; see
[/etc/default/pcscd](../../../../docs/kfs/16-wiki/fingerprint-fido-smartcards.md#etcdefaultpcscd).

`postinstall.sh` creates the `pcscd` system group and account, the ones upstream's `sysusers.d`
file declares; ccid's udev rule gives readers to that group. `47_pcscd` runs `pcscd --foreground`
as root under the supervisor, after `polkitd`, because every client is authorised through polkit
and a daemon started before it refuses them all.
