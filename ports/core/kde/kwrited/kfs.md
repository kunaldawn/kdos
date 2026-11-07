## About

kwrited receives the messages `write` and `wall` send to a user and shows each one as a desktop
notification. It opens a pseudo-terminal and registers it in utmp as one of the user's logins, so
that a program writing to every terminal of that user writes to it too; Plasma starts it from the
XDG autostart entry `kwrited-autostart.desktop`, phase 1.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D BUILD_AS_EXECUTABLE=ON` — Build kwrited as the program `/usr/bin/kwrited` rather than as a
  kded module. The configure forces this whenever [kpty](#port-kpty) lacks libutempter, as the
  `kpty` port does; naming it keeps the package the same shape when the kpty it builds against has
  utempter, where the default is the module. See Notes.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

With libutempter, [kpty](#port-kpty) registers a terminal through utempter's helper and kwrited can
be a kded module. The `kpty` port is built without utempter, so kwrited is a program of its own,
`/usr/bin/kwrited`, which upstream's install makes setuid root (`chown root`, `chmod u+s`) so that
it can write its utmp entry. `build.sh` clears the setuid and setgid bits after the install. kpty
writes that entry with the C library's `pututxline()`, which in musl does nothing, so the bit would
make kwrited a setuid-root program and register nothing.

The result is that kwrited runs and receives no message: `wall` from [util-linux](#port-util-linux)
reads the utmp records through [utmps](#port-utmps), and kwrited's terminal is not among them.
kwrited can receive messages once kpty writes its records through utmps; `utmps-utmpd` accepts a
record only from root or from a client running with the daemon's own group, so kwrited then needs
its setuid bit, or a setgid bit for that group, and the list of
[setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries) names
it.
