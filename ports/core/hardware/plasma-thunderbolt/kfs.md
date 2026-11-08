## About

plasma-thunderbolt is Plasma's front end to [bolt](#port-bolt), the Thunderbolt device manager: the
Thunderbolt page of System Settings (`kcm_bolt`), which authorises, trusts and forgets devices,
and a module of the KDE daemon (`kded_bolt`) that raises a notification when an unauthorised
device is plugged in and offers to authorise it. Both talk to `boltd` over
`org.freedesktop.bolt` on the system bus through the port's own `libkbolt`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[bolt](#port-bolt) and [kded](#port-kded) are in `depends` for run time. `boltd` is started by the
system bus on the first call, through the `Exec=/usr/libexec/boltd` line of its service file;
bolt's boot script makes that call at start-up on a machine with a Thunderbolt controller, and
bolt's udev rule makes it when a controller appears later. bolt's
polkit rule lets a member of `wheel` in the active local session enrol, authorise and manage
devices without a password; anyone else is asked for an administrator's. The kded module is a
plugin that `kded6` loads at login; without it nothing announces a new device.

`FindBolt.cmake` looks for `boltd` under `/usr/lib/bolt` and on the program search path, not in
`/usr/libexec`, so the configure reports bolt as missing even with it installed. The check is a
run-time note only and changes nothing in the build.
