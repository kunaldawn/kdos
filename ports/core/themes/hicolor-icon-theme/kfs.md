## About

hicolor is the fallback theme of the freedesktop.org icon theme specification: every icon lookup
that the current theme and its parents cannot answer ends in `/usr/share/icons/hicolor`, and an
application installs its own icon there. This package holds no icons. It installs
`index.theme`, which lists the size and context directories and makes the directory a theme, and
creates those directories, empty.

## Notes

Ports that install their icons under `/usr/share/icons/hicolor` name this one in `depends`, so
the theme's `index.theme` is in place by the time their icons are. The theme's `icon-theme.cache` belongs to no
package: `kpkg` writes it after each install that touches the theme, and the install that places
`gtk-update-icon-cache` writes it for every theme already present, this one among them (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)). KDOS's own
icon theme inherits from hicolor last, after Breeze and Adwaita (see
[Theming](../../../../docs/kdos/02-user-guide/theming.md)).
