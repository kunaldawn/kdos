## About

kactivitymanagerd is the session daemon behind Plasma's activities and its usage statistics: it
owns `org.kde.ActivityManager` on the session bus, keeps the list of activities and the one that is
current, and records which files and applications are opened, in an SQLite database under
`~/.local/share/kactivitymanagerd`, so that Kickoff's recent and frequent lists and KIO's `recentlyused:/`
can be built. [plasma-activities](#port-plasma-activities) and
[plasma-activities-stats](#port-plasma-activities-stats) are its client libraries.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The daemon is started by D-Bus activation. The install writes
`/usr/share/dbus-1/services/org.kde.ActivityManager.service` with an `Exec=` line naming
`/usr/lib/libexec/kactivitymanagerd`, which the session bus runs the first time a client calls the
name, and a `SystemdService=` line that only a bus started by systemd reads. It also installs the
systemd user unit that line names under `/usr/lib/systemd`, which the package lint refuses (see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses));
`build.sh` removes the directory and the activation file is what starts it.

[boost](#port-boost) is needed at build time only, for its header-only containers; nothing links
a Boost library.
