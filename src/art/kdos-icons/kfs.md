## About
`kdos-icons` is KDOS's two icon themes and its marks. `KDOS` (for the seven dark accents) and
`KDOS-Light` (for `paper`) are thin themes over Breeze: each holds only the KDOS penguin under the
names Plasma draws the KDE logo by — `start-here-kde`, `start-here-kde-symbolic`,
`start-here-kde-plasma` and `start-here-kde-plasma-symbolic`, so Kickoff, Kicker and the window
list show the penguin wherever they would show KDE's — and inherits every other icon from
`breeze-dark` or `breeze`. The marks themselves go into `hicolor`, where any theme finds them:
`start-here-kdos` (the penguin, the Kickoff button the global themes set), and
`distributor-logo-kdos` and `kdos` (penguin and wordmark, the About page's logo and
`/etc/os-release`'s `LOGO`).

It also ships `/usr/share/icons/default/index.theme`, which names `breeze_cursors`: an application
that names no cursor theme of its own, Xwayland's among them, reads that file. How the themes
follow the accent is in
[How the theme is generated](../../../docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Notes
Both `index.theme` files set `FollowsColorScheme=true`, and that is what makes the accent reach the
icons at all. KIconLoader recolours an SVG that carries a `current-color-scheme` stylesheet only
when the TOP theme's own `index.theme` asks for it; an inherited Breeze icon follows the colour
scheme through `KDOS` only because `KDOS` says so. GTK, libadwaita and other non-KDE toolkits do no
such recolouring and draw those icons in their baked-in Breeze blue.

The size keys (`DesktopDefault`, `PanelSizes` and the rest) repeat Breeze's. KIconTheme reads them
from the top theme and falls back to its own built-in sizes, not to an inherited theme's, so a
theme without them draws desktop and panel icons smaller than Breeze would.

The theme inherits Breeze but does not name `breeze-icons` in `depends`: that port is in
43_toolkits and this one in 41_system, and a 41 port's closure must not reach a toolkit. The
`kde-plasma` meta port installs Breeze on every image that has a desktop.

The marks in `marks/` are cut from the master artwork, `kdos.png` at the top of the tree, by
`genmarks.py` on a development host (it needs Pillow); the build only copies the PNGs it committed.
They are raster, so they are the one part of these themes that does not follow the accent.

This port installs before `gtk3`, so it has no `icon-theme.cache` until gtk3 installs
`gtk-update-icon-cache`, and that install writes one for every theme then present; see
[Shared indexes](../../../docs/kdos/05-developer/writing-ports.md#shared-indexes).
