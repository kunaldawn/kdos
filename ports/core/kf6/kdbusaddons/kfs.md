## About

KDBusAddons is the KDE Frameworks library of conveniences over Qt D-Bus: `KDBusService`, which
registers an application's unique bus name so a second launch hands its arguments to the running
instance; `KDEDModule`; and helpers for D-Bus activation. Its `kquitapp6` command asks a running
application to quit over the bus. [kio](#port-kio) and many KDE applications link it.

## Flags

- `-D WITH_X11=ON` — Build the X11 code, which passes the launch's startup identifier to the running
  instance for a program under Xwayland. It is upstream's default on Linux.
