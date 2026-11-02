## About

KDiamond is a match-three game: swap two neighbouring diamonds to make a line of three or more of
one colour, against the clock or without one. Game events are notifications, so their sounds are
set in its Configure Notifications dialog.

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
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.

## Notes

Its three events, removing a line, a move and the end of a game, are set to play a sound and
nothing else; [knotifications](#port-knotifications) plays it through
[libcanberra](#port-libcanberra), and a popup turned on in the dialog goes to the session's
notification server. The two stone sounds are files the package installs under
`/usr/share/sounds`; the game-over sound is the sound-theme name `game-over-winner`, which the
session's theme, [ocean-sound-theme](#port-ocean-sound-theme), carries.

The window's Wayland app_id is `org.kde.kdiamond`: `KAboutData` builds that desktop file name from
the component name and its organisation domain, `kde.org`. It is the desktop entry's file name too,
and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.
