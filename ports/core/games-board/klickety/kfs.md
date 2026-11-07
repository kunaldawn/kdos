## About

Klickety is an adaptation of the Clickomania game: click a group of two or more touching blocks of
one colour to remove it, and clear as much of the board as possible. Started with `--KSameMode` it
plays the KSame rules, scoring by group size, which upstream offers as a second menu entry,
SameGame (`org.kde.ksame.desktop`).

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

The window's Wayland app_id is `org.kde.klickety`: `KAboutData` builds that desktop file name from
the component name and its organisation domain, `kde.org`. It is the desktop entry's file name too,
and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.

The SameGame entry starts the same program with `--KSameMode`, and its window carries the same
app_id, so the task manager shows a SameGame window under Klickety's entry and icon.

The package installs a configuration update, `klickety.upd`, whose script
`klickety-2.0-inherit-ksame-highscore.pl` copies the `[KHighscore]` group of a `ksamerc` into
`klicketyrc` as `[KHighscore_KSame]`. `kconf_update` runs the script with `perl`, from
[perl](#port-perl), which is in `depends` for that reason.
