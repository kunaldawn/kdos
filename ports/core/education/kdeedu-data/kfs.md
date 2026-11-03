## About

kdeedu-data is the data the KDE Education programs share: KVTML vocabulary documents for 37
languages and script variants under `/usr/share/apps/kvtml/<language>`, and a set of hicolor
action icons. Nothing in it is compiled. [kanagram](#port-kanagram) and
[khangman](#port-khangman) draw their words from the vocabularies, through
[libkeduvocdocument](#port-libkeduvocdocument)'s shared-file lookup, and [kmplot](#port-kmplot)
its toolbar from the icons.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as in every KDE recipe, so that the English-only rule
  holds for a later release that adds catalogues (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release has none, and CMake reports the variable as unused.

## Notes

Nothing is compiled, but [qt6-qtbase](#port-qt6-qtbase) is in `depends`: ECM's
`KDEInstallDirs` asks Qt's `qtpaths` for its prefix and the configure stops without it, and that
puts the port in `43_toolkits`.

The vocabularies in other languages are word lists for learners, not translations of an
interface, so the language rule keeps them all. `apps/kvtml` under the generic data directory is
the path libkeduvocdocument's shared-file lookup searches, so the files cannot move to
`/usr/share/kdeedu-data` without the library following.
