## About
`kdos-theme` is the build-time generator of KDOS's identity kit. Every colour KDOS hands to KDE is
written by it, from the eight schemes of libkcolor's `KCOL_SCHEMES` and from nothing else, so the
boot menu, the console, the greeter and the desktop cannot disagree about what an accent is. It
runs in the `build.sh` of the ports that install what it writes, never in a session:

| Subcommand | Writes | For |
|---|---|---|
| `kde-colors <outdir>` | `KDOS<Title>.colors`, one KColorScheme file per accent, and `KDOSBox.colors` for a box with none | `/usr/share/color-schemes`, installed by [kdos-plasma-look](#port-kdos-plasma-look) |
| `syntax-themes <outdir>` | `KDOS-<Title>.theme`, one KSyntaxHighlighting theme per accent | `/usr/share/org.kde.syntax-highlighting/themes`, installed by this port |
| `konsole <outdir>` | `KDOS-<Title>.colorscheme` and `KDOS-<Title>.profile` per accent | `/usr/share/konsole`, installed by kdos-plasma-look |
| `qt5ct-colors <outdir>` | `KDOS-<Title>.conf`, the Qt 5 palette per accent | `/usr/share/qt5ct/colors`, installed by kdos-plasma-look |
| `wallpaper <outdir> --src MARK.png` | `KDOS-<Title>/`, a Plasma wallpaper package per accent | `/usr/share/wallpapers`, installed by [kdos-wallpapers](#port-kdos-wallpapers) |
| `greeter-globals <file> [accent]` | the colour groups, merged into an existing kdeglobals | the kdeglobals a process outside a Plasma session reads |
| `greeter-globals --sddm <outdir>` | `<accent>/{kdeglobals,plasmarc,kcminputrc}` per accent, and `background.png`, a link to the dark picture of the accent the greeter shows | the three files the sddm user's `~/.config` takes for the system accent, and the greeter's background |
| `contrast FILE…` | a table, and exit status 1 when any role is below its floor | the gate in `testing/selftest.sh`, and any `build.sh` that installs a scheme |
| `accents [--long \| --default]` | the accent names; `--long` adds the title, `light` or `dark`, and the scheme the greeter shows | a `build.sh` that makes one file per accent |

With no accent named, a generator writes all eight. The KDE role mapping, the contrast floors,
why a dark accent's selection carries `text` and why the window ground is `deep` moved towards
grey are in
[How the theme is generated](../../../docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Flags
- `-std=gnu11`, `-D_GNU_SOURCE` — The C dialect, and the macro that exposes the C library's GNU and
  POSIX extensions to the program and the two libraries compiled with it. The tree's `CFLAGS`
  already names the same `-std`.
- `-I"$LIBS/libkbase"`, `-I"$LIBS/libkcolor"`, `-I"$PORT_SRC"` — The headers of the two
  libraries it uses and of the program itself. `$LIBS` is `src/libs`, reached from the port
  directory.
- `"$LIBS"/libkbase/*.c`, `"$LIBS"/libkcolor/*.c` — Compile both libraries into the binary. A
  `libk*` library is a directory of sources, never an installed archive; see
  [How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
- `-lpng` — The wallpaper generator reads the mark and writes the wallpapers through libpng, which
  is in `depends`.
- `-lm` — The C library's maths: the model of the shades KColorScheme derives from a window
  background works on a 2.2 power curve, and the chroma it is measured by on a cube root.
- `-O2`, `-Wall`, `-Wextra` — The tree's optimisation level, named again, and the compiler's
  common and extra warnings.

## Notes
`kde-colors` and both forms of `greeter-globals` measure the palette before they write a byte and
die when a role misses its floor: 4.5:1 for every text role and 3:1 for every decoration, each
against both backgrounds of its set, 4.5:1 for the selection's text on the Selection's hover,
which GTK fills a hovered selected row with, 4.5:1 for the view's text over the selection laid at 64% over
the view, 4.5:1 for the window titles, and a chroma ceiling on the five shades KColorScheme derives
from the window background. `kde-colors` also dies when `KDOSBox`'s title bars measure under 1.5:1
against any accent's, and `syntax-themes` when a syntax colour misses 4.5:1 on the editor or the
selection. A scheme that fails is therefore a failed build of the port that installs it, never an
installed scheme. `contrast` measures the files as written, so a file edited after it was generated
is measured as it is, and it derives the shades at the file's own `[KDE] contrast`, or at KDE's
default of 7 when the file carries none, which is what KColorScheme does. `greeter-globals` writes
`[KDE] contrast` into the `kdeglobals` it merges for that reason.

The shade model in `shade.c` is KColorScheme's `shade()` over KGuiAddons' KHCY colour space,
step for step, to the 8-bit colour Qt paints. It has to move with KColorScheme: a release that
changes the derivation leaves the gate measuring colours no program draws.

`build.sh` runs the binary it has just linked to write the editor themes, so this port, unlike the
other generators' consumers, installs files of its own beside `/usr/bin/kdos-theme`.

The recipe hash of a port of KDOS's own covers its own directory and all of `src/libs`, and
nothing else. An edit to libkcolor's palette therefore rebuilds this port and every port that runs
it, but an edit to this program's own sources rebuilds this port alone (and its editor themes):
kdos-plasma-look and kdos-wallpapers keep the files the old generator wrote until they are rebuilt
with it
(`--rebuild kdos-theme,kdos-wallpapers,kdos-plasma-look`). A colour that appears in a shipped KDE
file and is not one of these generators' outputs is a second copy of the palette.

The sixteen terminal colours of the Konsole schemes are libkcolor's `kcol_ansi()`, the same
derivation `kdos theme` writes into tmux, btop and the other terminal programs, so a terminal
program's status line and the terminal under it agree about blue. The profiles set `WordModeCoding=false`: Konsole otherwise draws the characters
programming ligatures are made of as one run, and JetBrains Mono joins `!=` or `->` into a single
glyph, where a terminal has to show what was typed.
