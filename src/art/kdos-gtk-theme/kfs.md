## About
`kdos-gtk-theme` is the GTK theme, `KDOS`, for the GTK 3 and GTK 4 applications on the machine and
in boxes. It is adw-gtk3's dark variant, the libadwaita stylesheet ported to GTK 3, chosen in
`vendor.py` because its stylesheet is written against GTK's named colours, so rewriting the colour
definitions reaches every widget. `theme/` holds the vendored stylesheets and assets, and
`build.sh` runs [kdos-theme](#port-kdos-theme) over them to write `/usr/share/themes/KDOS` in the
`phosphor` accent: each literal colour hue-mapped by family with its lightness kept, then the
semantic names (window, view, headerbar, accent and the rest) set to KDOS values outright.
`LICENSE.notice` lists every way the result differs from upstream.

The desktop itself never reads this theme: KDOS's own programs draw cells and link no toolkit.
The vendored stylesheet is installed again under `/usr/share/kdos/gtk-theme/theme`, because
`kdos theme <accent>` regenerates the theme into each home directory from it; see
[How the theme is generated](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Flags
- `phosphor` — The accent the system copy is generated in. The copy a user's applications load
  is the one `kdos theme` writes into their home directory, in their own accent.
- `--src` — Read the port's own `theme/` rather than the installed
  `/usr/share/kdos/gtk-theme/theme`, which this package is about to install.

## Notes
Only the dark variant is vendored; KDOS has no light mode. Upstream's dark `gtk-3.0/gtk.css` is
byte-identical to its `gtk-dark.css`, so only the second is kept, and the generator writes both
names from it.

The assets (slider marks, selection handles) are installed as upstream ships them. Each is neutral
grey or a symbolic mask that GTK tints itself, so none carries a colour to remap.

Unlike [kdos-cursors](#port-kdos-cursors), this port puts nothing in `/etc/skel`. The image step
`script/phases/70_image/050_theme.sh` runs `kdos theme` against `/etc/skel` in the default accent,
which writes the theme a new account starts with.

`vendor.py` re-vendors `theme/` from an adw-gtk3 release on a development host; the build never
runs it.
