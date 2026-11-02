## About

Knights is KDE's chess board. It plays against a local chess engine over the XBoard or UCI protocol,
two people at one desk, two engines against each other, or opponents on the Free Internet Chess
Server, with clocks, themes and spoken moves. It is a Qt 6 and KDE Frameworks application built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6TextToSpeech=ON` — Make Qt TextToSpeech a requirement.
  Upstream looks for it quietly, and without it the accessibility setting that speaks the
  opponent's moves is compiled out with nothing said. It comes from
  [qt6-qtspeech](#port-qt6-qtspeech), which is in `depends` and speaks through
  [speech-dispatcher](#port-speech-dispatcher).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the themes' SVGs to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

The configure requires Plasma: the player clocks draw `widgets/clock` from the Plasma desktop theme
through [ksvg](#port-ksvg), and that theme is installed by [libplasma](#port-libplasma). Both are
`43_toolkits` libraries, so Knights stays a `44_apps` port.

Knights runs no engine of its own. Of the engines its default configuration names (GNU Chess,
Crafty, Stockfish and Sjeng), [stockfish](#port-stockfish) is the one that is a port, and it is in
`depends` for run time; the build does not look for it. Upstream's default opponent is GNU Chess,
which the new-game dialog then offers first and which fails to start, so
`/usr/share/kdos/xdg/knightsrc` sets `player1Program` and `player2Program` in `[Protocols]` to
`Stockfish`. The engine settings page marks each configured command as found or missing on
`$PATH`.

A Free Internet Chess Server password is kept in the wallet through [kwallet](#port-kwallet).

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.knights.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.knights`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.
