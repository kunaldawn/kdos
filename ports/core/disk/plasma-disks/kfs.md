## About

plasma-disks watches the health of the machine's drives. A kded module asks Solid for every disk,
reads each one's SMART status through `smartctl` from [smartmontools](#port-smartmontools), and
raises a notification when a drive reports itself failing or past an alarming threshold; the
SMART Status page of Info Center shows the full report.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Reading SMART data needs root, so the module calls `kded-smart-helper`, a KAuth helper that runs
`smartctl --all --json=c` with `PATH` set to `/usr/sbin:/sbin:/usr/local/sbin`. smartmontools
installs `smartctl` in `/usr/sbin`, which is the first entry; a copy anywhere else is not found.
The helper's polkit action, `org.kde.kded.smart.smartctl`, is granted by upstream's own policy
to active and inactive local sessions, so the checks the module makes at login, once a day and for
each drive plugged in ask for no password in a session elogind tracks (see
[The security model](../../../../docs/kdos/03-architecture/security-model.md)).

The configure looks for `smartctl` only to report it. Disks are enumerated through Solid's UDisks2
backend, so [udisks2](#port-udisks2) is in `depends` for run time; without it no drive is listed.
