## About

KFourInLine is four in a row: drop pieces into a seven-column board until one player has four in a
line. It plays against a computer opponent at several strengths, or against another person at the
same screen or over the network. The computer player is a program of its own, `kfourinlineproc`,
which the game drives over its standard input and output.

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

The game looks for `kfourinlineproc` first at `kwin4/kfourinlineproc` under its working directory
and then on `PATH`, so a game started from a directory that holds such a file runs that file as the
computer player.

The connect dialog finds network games on the local network through KDNSSD from
[kdnssd](#port-kdnssd), which talks to the avahi daemon of [avahi](#port-avahi) over D-Bus; mDNS,
UDP 5353, passes the shipped firewall. A machine that hosts a game listens on TCP 7442, which the
firewall's input chain drops until a rule opens it (see [The
firewall](../../../../docs/kdos/03-architecture/security-model.md#the-firewall)); joining a game
another machine hosts needs no rule.

The window's Wayland app_id is `org.kde.kfourinline`: `KAboutData` builds that desktop file name
from the component name and its organisation domain, `kde.org`. It is the desktop entry's file name
too, and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its
entry without a patch.
