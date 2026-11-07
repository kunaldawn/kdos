## About

Kiriki is a Yahtzee-style dice game for up to six players, each one a person or a computer player:
roll five dice up to three times a turn and fill the score sheet. The score sheet can be printed,
through Qt Print Support.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  `kdoctools_install()` still builds the handbook in every translated language, and kpkg's
  language rule removes `/usr/share/doc/HTML/<lang>/` from the package.

## Notes

Printing goes through Qt's CUPS backend, which [qt6-qtbase](#port-qt6-qtbase) is built with. Kiriki
ships no SVG themes, so it has no SVGZ step and no gzip requirement.

The window's Wayland app_id is `org.kde.kiriki`: `KAboutData` builds that desktop file name from
the component name and its organisation domain, `kde.org`. It is the desktop entry's file name too,
and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.
