## About

Kolor Lines is a Color Lines game: move a ball to any square it can reach, line up five of one
colour to remove them, and keep the board from filling as three new balls arrive after every move
that completes no line.

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

The window's Wayland app_id is `org.kde.klines`: `KAboutData` builds that desktop file name from
the component name and its organisation domain, `kde.org`. It is the desktop entry's file name too,
and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.
