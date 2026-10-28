## About
ccid is the [pcsc-lite](#port-pcsc-lite) driver for USB smart card readers that follow the USB
CCID and ICCD specifications, which covers most USB readers and the smart card interface of
security keys such as YubiKeys. pcscd loads it from its bundle under `/usr/lib/pcsc/drivers`. The
port also builds the driver for serial CCID readers, `libccidtwin`, and installs the udev rule
that gives readers to the `pcscd` group.

## Flags
- `-Dpcsclite=true` — Log through pcsc-lite's debug functions, which pcscd provides, instead of
  compiling the driver's own `debug.c`; upstream's default, written out.
- `-Dserial=true` — Also build `libccidtwin`, the driver for serial CCID readers, with its
  `reader.conf.d` entry. Upstream's default is off.
- `-Dclass=true` — Declare `CCIDCLASSDRIVER` in the bundle's `Info.plist`, so pcscd uses the
  driver for any USB device of the CCID class, not only for the readers the driver lists by
  vendor and product; upstream's default, written out.
- `-Dcomposite-as-multislot=false`, `-Dzlp=false`, `-Dembedded=false`, `-Dos_log=false` —
  upstream's defaults written out: a composite device is not presented as one multi-slot reader,
  no Zero Length Packet workaround for certain Gemalto readers, logging kept, and no macOS
  `os_log`.
- `-Denable-extras=false` — Build none of the examples and contrib programs, among them
  `Kobil_mIDentity_switch`, which meson would build but not install.
- `-Dudev-rules=false` — Leave the udev rule to `build.sh`, which installs
  `src/92_pcscd_ccid.rules` into `/usr/lib/udev/rules.d` itself. meson would install it in the
  `udevdir` that [eudev](#port-eudev)'s `udev.pc` names, `/lib/udev`.

## Patches
- `no-kobil-switch.patch` — Removes from `92_pcscd_ccid.rules` the rule that runs `/usr/sbin/Kobil_mIDentity_switch` when a Kobil mIDentity (USB `0d46:4081`) is plugged in. That program is one of the contrib extras, which this build does not install, so the rule would run a program that is not there.

## Notes
The rule file gives every USB device with a CCID interface, and each listed reader, the group
`pcscd`, which [pcsc-lite](#port-pcsc-lite)'s `postinstall.sh` creates, and turns off USB
persist for CCID devices.
