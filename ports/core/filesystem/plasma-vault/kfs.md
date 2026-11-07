## About

Plasma Vaults are encrypted folders a person opens and closes from the system tray. A module of
the KDE daemon (`kded_plasmavault`) creates, mounts and unmounts them, the tray applet lists
them, and a Dolphin context-menu plugin opens one from its encrypted directory. A vault can be
tied to activities, and an offline vault turns networking off while it is open. The encryption is
[gocryptfs](#port-gocryptfs) over FUSE.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6NetworkManagerQt=ON` — Offline vaults, which switch
  NetworkManager's networking off while they are open, through
  [networkmanager-qt](#port-networkmanager-qt). Upstream links the framework only when it is
  found, and without it the wizard still offers an offline vault whose switch does nothing, so
  the network stays up while the vault is open.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The creation wizard makes gocryptfs vaults only. The import wizard also lists CryFS and EncFS,
whose tools are not ports, so importing a vault of either kind reports its backend program
missing. A vault is mounted by running `gocryptfs` and unmounted with `fusermount3` from
[fuse](#port-fuse); both are in `depends` for run time, together with [kded](#port-kded), whose
`kded6` loads the module at login, and [kirigami](#port-kirigami) for the applet. The configure's
search for `gocryptfs` is a run-time note and changes nothing in the build.

Closing a vault that a program still uses offers to force it closed: the module asks
[lsof](#port-lsof) which processes hold files under the mount point and ends each one with
`SIGKILL` through [libksysguard](#port-libksysguard). lsof is in `depends` for that run-time
call; without it the forced close reports that it cannot list the applications.
