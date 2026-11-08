## About

Naval Battle (`knavalbattle`) is KDE's Battleship: each side places its ships on a hidden grid and
the two take turns firing at the other's. It plays against the computer, or against a second player
over the network. It is a Qt 6 and KDE Frameworks application built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the theme's SVG to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

A network game is announced and found through DNS-SD, by [kdnssd](#port-kdnssd) over the Avahi
daemon that `45_avahi.sh` starts; without it, the other player is reached by host name and port. The
player who hosts listens on TCP port 54321 by default, which the input chain of
`/etc/nftables.conf` drops, so hosting needs a rule accepting that port in a file of its own under
`/etc/nftables.d`; joining someone else's game is an outgoing connection and needs nothing. The desktop entry claims the `kbattleship:` URL scheme,
so a link of that form opens a network game.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.knavalbattle.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.knavalbattle`, from the component name and the `apps.kde.org` home page, so
it is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.
