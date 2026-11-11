## About
`kdos-icons` is KDOS's two icon themes and its marks. `KDOS` (for the seven dark accents) and
`KDOS-Light` (for `paper`) are thin themes over Breeze. Each holds the KDOS penguin under the
names Plasma draws the KDE logo by — `start-here-kde`, `start-here-kde-symbolic`,
`start-here-kde-plasma` and `start-here-kde-plasma-symbolic`, so Kickoff, Kicker and the window
list show the penguin wherever they would show KDE's — and two links into Breeze: `im-google`,
the icon Online Accounts' Google provider names and no Breeze release draws, to
`internet-services`, and `kgpg` to Breeze's monochrome `kgpg-symbolic`, so KGpg follows the scheme
instead of showing its hicolor padlock, drawn dark for a light panel. `KDOS` also carries Breeze
Dark's status, emblem and package marks recoloured (below). Every other icon is inherited from
`breeze-dark` or `breeze`, then Adwaita, then `hicolor`. The marks themselves go into `hicolor`,
where any theme finds them:
`start-here-kdos` (the penguin, the Kickoff button the global themes set), and
`distributor-logo-kdos` and `kdos` (penguin and wordmark, the About page's logo and
`/etc/os-release`'s `LOGO`). Both themes also carry `start-here-kdos` beside `start-here-kde`.

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

Breeze draws its status, emblem and package marks (`data-information`, `data-success`,
`emblem-*`, `vcs-*`, `package-*` and the rest, with their `dialog-*` aliases) as a shape in
`ColorScheme-Accent`, `-Highlight`, `-PositiveText`, `-NeutralText` or `-NegativeText` under a
glyph fixed `#fff`. Every dark KDOS accent makes those shape colours light, and the glyph
disappears. `themes/KDOS/` therefore carries a copy of each, made by `genstatus.py` on a
development host from an installed Breeze Dark (`--src /usr/share/icons/breeze-dark --out
themes/KDOS`): the opaque white glyph becomes `ColorScheme-Background` on a positive, neutral or
negative shape and `ColorScheme-HighlightedText` on an accent or highlight one, the two colours
those shapes are solved against. The aliases Breeze links to a copied icon are linked beside it, a
lookup by alias otherwise reaching the white original, and so are the `@2x` and `@3x` directories.
The script rewrites the end of `themes/KDOS/index.theme` below its marker line. The copies are
Breeze's artwork, which is why the recipe's licence names LGPL-3.0-or-later beside MIT; a Breeze
release that redraws those icons is shown in its older drawing until the script is run again.
`KDOS-Light` needs none: `paper`'s
accent and status colours are dark, and the white glyph holds on them.

KIconLoader tries a name and then its dash-stripped forms in each theme before it moves to the
next, so a name a theme above `hicolor` answers in a shorter form never reaches `hicolor`:
`start-here-kdos` would be answered by AdwaitaLegacy's `start-here`, GNOME's foot, under Adwaita,
which is why both themes carry their own copy. `genstatus.py` for the same reason copies no name
Breeze Dark draws in two contexts: `checkmark` is an action, the plain tick, and an emblem, the
filled badge, and a copy of the emblem in `KDOS` would answer `checkmark` and
`checkmark-symbolic` everywhere. For the same reason each Breeze Dark name that extends a copied one
(`printer-error-symbolic`, `emblem-important-symbolic`, `package-install-auto`) is linked in
`KDOS` to Breeze Dark's own file; the copy of `printer-error` would otherwise answer it.

Adwaita comes after Breeze in both themes' `Inherits` for the symbolic names only Adwaita draws,
such as `sidebar-show-right-symbolic`, which GTK programs ask for; without it they draw the
missing-image icon.
