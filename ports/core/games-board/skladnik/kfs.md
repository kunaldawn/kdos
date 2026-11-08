## About

Skladnik is Sokoban, the Japanese warehouse-keeper puzzle: the keeper pushes gems, one at a time and
never pulling, until every gem sits in the store room. It carries the classic level collections and
opens others from files. It is a Qt 6 and KDE Frameworks application built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook and the `skladnik(6)` manual page,
  through [kdoctools](#port-kdoctools), which upstream looks for as an optional package: without it
  the `doc` directory is skipped with nothing said. Required, a missing kdoctools stops the
  configure.
- `-D CMAKE_DISABLE_FIND_PACKAGE_POVRay=ON` — Install the game's pictures as the release carries
  them. The release holds the PNGs and the POV-Ray scenes they are rendered from; when upstream's
  `FindPOVRay` finds a `povray`, `src/images` renders every PNG anew into the source directory.
  POV-Ray is not a port, so the switch keeps the build what it is whatever a build root holds.

## Notes

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook and the manual page in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` and every directory under `/usr/share/man` that is not a
`man<section>` one from the package.

The desktop entry is upstream's `org.kde.skladnik.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.skladnik`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.
