## About

Kigo is KDE's Go board game. It draws the board and stones and speaks the Go Text Protocol (GTP) to
a separate engine process, which plays the computer's moves, scores the game and gives hints;
[gnugo](#port-gnugo) is that engine by default.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  `kdoctools_install()` still builds the handbook in every translated language, and kpkg's
  language rule removes `/usr/share/doc/HTML/<lang>/` from the package.
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compresses the theme SVGs to SVGZ with gzip.
  Upstream's `cmake/InternalMacros.cmake` uses 7-Zip's `7zz` whenever it finds one and otherwise
  requires gzip; [7zip](#port-7zip) is built in an earlier phase, so without the switch the package
  would follow whatever the build root holds. gzip runs with `-n`, which writes no file name or time
  stamp into the SVGZ files.

## Notes

The engine is a run-time dependency only. The configure looks for `gnugo` quietly and builds the
same program whether it finds one or not; the default engine command, `gnugo --mode gtp`, is
resolved on `PATH` when Kigo first reads its configuration, and without an engine Kigo cannot start
a game. [gnugo](#port-gnugo) is in `depends` for that reason, and any other GTP engine can be named
in the settings instead.

Get More Games, through [knewstuff](#port-knewstuff), downloads game records from the KDE store
only when the player asks; KNewStuff is a requirement of the configure with no switch.

The window's Wayland app_id is `org.kde.kigo`: `main.cpp` sets that desktop file name itself. It is
the desktop entry's file name too, and upstream's entry sets no `StartupWMClass`, so the task
manager pairs the window with its entry without a patch.
