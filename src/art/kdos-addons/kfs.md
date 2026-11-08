## About
`kdos-addons` is the curated set KDOS offers in place of KDE's online "Get New…" store, which is
switched off on every machine (`[KDE Action Restrictions][$i] ghns=false` in
`/usr/share/kdos/xdg/kdeglobals`). It is installed with the desktop, and every file goes where
Plasma already looks, so the Wallpaper, Colours, Konsole, KWin Scripts and Add Widgets pages list
it from the first login with no code of KDOS's in between. Nothing in it is switched on: the
look stays the accent's until a person picks an item, the two KWin scripts ship disabled and the
two widgets wait in Add Widgets. Discover lists the set under Add-ons through its AppStream
entry as installed host software; it is a kpkg port, which Discover cannot remove.

| Installed under `/usr/share/` | What |
|---|---|
| `wallpapers/org.kdos.{scanlines,blueprint,horizon,eight}/` | four wallpaper packages, an SVG each for light (`images/`) and dark (`images_dark/`), so each follows Plasma's light/dark switch |
| `color-schemes/<Name>.colors` | eight KDE colour schemes: VGA, Nord, Dracula, Gruvbox Dark, Gruvbox Light, Tokyo Night, Catppuccin Mocha, Catppuccin Latte |
| `konsole/<Name>.colorscheme` | the same eight palettes as Konsole schemes |
| `kwin/scripts/org.kdos.pinpip/` | keeps a browser's picture-in-picture video above every window, on every desktop |
| `kwin/scripts/org.kdos.activescreen/` | opens each new window on the screen under the pointer |
| `plasma/plasmoids/org.kdos.plasma.phosphorclock/` | the time in the fixed-width face, its separator in the accent |
| `plasma/plasmoids/org.kdos.plasma.focustimer/` | focus periods and breaks, with a notification at each change |
| `knotifications6/kdos-addons.notifyrc` | the Focus Timer's notification, listed on the Notifications page |
| `metainfo/io.kdos.addons.metainfo.xml` | the AppStream entry Discover shows, an add-on of `org.kde.plasmashell` |

Everything is MIT: KDOS's own work, and the five palettes the colour schemes are made from, whose
copyright lines `LICENSE.notice` carries and the package installs to
`/usr/share/licenses/kdos-addons/`.

| Palette | Copyright | Source |
|---|---|---|
| VGA | KDOS, from the IBM VGA text-mode palette | — |
| Nord | Arctic Ice Studio and Sven Greb | <https://www.nordtheme.com> |
| Dracula | Dracula Theme | <https://draculatheme.com> |
| Gruvbox Dark, Gruvbox Light | Pavel Pertsev | <https://github.com/morhetz/gruvbox> |
| Tokyo Night | Folke Lemaitre | <https://github.com/folke/tokyonight.nvim> |
| Catppuccin Mocha, Catppuccin Latte | Catppuccin | <https://github.com/catppuccin/catppuccin> |

## Notes
Nothing generated is committed. `genaddons.py` runs in `build.sh` with python3 alone and writes
the sixteen scheme files and the eight wallpaper SVGs from `palettes.txt` and from libkcolor's
`KCOL_SCHEMES` in `src/libs/libkcolor/kcolor.h`, which it parses as text: the wallpapers are drawn
in the KDOS accents, so an accent retuned there is retuned in every wallpaper at the next build,
and editing `kcolor.h` rebuilds this port with every other port of KDOS's own.

Every text role of every KDE scheme is held at WCAG AA, 4.5:1, against both backgrounds of its
colour set, and every decoration at 3:1. A palette's published colour that falls short is moved
toward its set's text colour in 1% steps until it passes, and a role that cannot reach the floor
fails the build; `python3 genaddons.py --kcolor … --palettes … --report` prints each moved role
with its ratio before and after. Low-contrast palettes move the most: Nord's red on its own grey
starts at 2.1:1, and Catppuccin Latte's orange and green on its light ground at 2.3:1 and
2.5:1.

No add-on is named with a `KDOS` prefix. A colour scheme called `KDOS*` would be taken for one of
the eight accents by `kdos theme` and by the accent watcher in kded, and a wallpaper package
called `KDOS-*` is the one `kdos theme` rewrites to the new accent's on every desktop showing it;
an add-on picked by hand must survive an accent switch, so `genaddons.py` refuses a palette id
that starts with `kdos`.

The wallpapers are SVG because Plasma renders a package's SVG at the size of each screen, so one
file serves every resolution; they use only what QtSvg draws (rectangles, paths, circles, linear
and radial gradients), with no filter, pattern or clip path. The KWin scripts are plain
JavaScript on KWin's scripting API, which does not change between Plasma releases the way an
effect's does, and both ship disabled (`EnabledByDefault: false`) for a person to turn on. The two
widgets use only Plasma's public applet API, Kirigami's theme colours and the KDE notification
module; their i18n domain is Plasma's own `plasma_applet_<id>`.
