## About

hicolor is the fallback theme of the freedesktop.org icon theme specification: an icon lookup
that the current theme and its parents cannot answer falls through to `/usr/share/icons/hicolor`,
and an application installs its own icon there. This package holds no icons. It installs
`index.theme`, which lists the size and context directories and makes the directory a theme, and
creates those directories, empty.

## Notes

Most ports that put an icon under `/usr/share/icons/hicolor` do not name this one in `depends`,
and several in `41_system` install before it, so the directory can exist without its
`index.theme` for part of the build. The theme's `icon-theme.cache` belongs to no package: `kpkg`
writes it after each install that touches the theme, only once the directory holds an
`index.theme`, and the install that places `gtk-update-icon-cache` writes it for every theme
already present, this one among them (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)). KDOS's own
icon theme inherits from hicolor last, after Breeze and Adwaita (see
[Theming](../../../../docs/kdos/02-user-guide/theming.md)).
