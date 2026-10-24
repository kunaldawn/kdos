## About
`kdos-icons` is the KDOS icon theme: Papirus, pruned and recoloured into the accent. `art/` holds
the vendored artwork (Papirus and Papirus-Dark merged into one theme, fewer sizes, no `apps/`
directory, one folder colourway), and `build.sh` runs [kdos-theme](#port-kdos-theme) over it to
lay out `/usr/share/icons/KDOS` in the `phosphor` accent. `LICENSE.notice` lists every way the
theme differs from upstream; the port's version is the Papirus release recorded in
`art/UPSTREAM`.

It also ships what the theme is regenerated from. `art/` and `marks/` are copied to
`/usr/share/kdos/icons/`, because `kdos theme <accent>` rebuilds the theme in your home directory
from them on every accent switch, and `atlas/atlas.kia`, the pre-rendered pixel icons the desktop
draws into the character grid through libkicon, goes beside them. The KDOS marks, the penguin and
the logo, are installed into `hicolor` as `kdos-launcher`, `start-here` and
`distributor-logo-kdos`. How the theme is generated and recoloured is in
[How the theme is generated](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Flags
- `phosphor` — The accent the system copy is generated in. The packaged themes are always
  `phosphor`; the theme a user sees is the copy `kdos theme` writes into their home directory.
- `--src` — The artwork to recolour, here the port's own `art/` instead of the installed
  `/usr/share/kdos/icons/art`, which does not exist yet while the package is built.
- `--marks` — The directory of KDOS marks to add to the theme, here the port's `marks/`.

## Notes
The marks go into `hicolor` because libkicon, which draws the desktop's pixel icons, reads its
atlas, then hicolor's `apps/` PNGs, then `pixmaps/`, and never the `KDOS` theme directory. Without
them the Start button draws its `≡` glyph.

The atlas is optional. `build.sh` installs `atlas.kia` only when it exists; without it the
desktop draws glyphs for the theme's own icons (folders, devices, file types) and still draws each
application's icon from hicolor. The atlas covers fewer names than `art/`:
a name can be in the artwork and still not be drawable on the desktop; see
[Desktop entries belong to the port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).

Nothing in the build rasterises or crops. `vendor.py` re-vendors `art/` from a Papirus release,
`genmarks.py` cuts `marks/` out of the master artwork with Pillow, and `genatlas.py` renders
`atlas.kia`; all three run on a development host, and their output is committed. The atlas is
stored in upstream's colours and tinted when libkicon loads it, so one file serves every accent.

This port installs before `gtk3`, so it has no `icon-theme.cache` until gtk3 installs
`gtk-update-icon-cache`, and that install writes one for every theme then present; see
[Shared indexes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#shared-indexes).
