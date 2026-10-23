## About
`kdos-cursors` is the pointer theme, `KDOS-cursors`: Bibata Modern Ice, pruned to the shapes and
sizes KDOS uses and recoloured into the accent. `art/` holds the vendored Xcursor files, still in
upstream's monochrome, and `build.sh` runs [kdos-theme](#port-kdos-theme) over them to lay out
`/usr/share/icons/KDOS-cursors`: each pixel's brightness mapped onto the accent's ramp, the busy
shapes onto the secondary colour, and every X11 name a symbolic link to the CSS-named file.
`LICENSE.notice` lists every difference from upstream, and `art/UPSTREAM` names the release.

The session exports `XCURSOR_THEME=KDOS-cursors` from `/etc/profile.d/10-wayland.sh`, and GTK's
`settings.ini` in `/etc/skel` names the same theme, so the compositor's pointer and the one an
application draws over its own window match. The vendored artwork is installed as well, under
`/usr/share/kdos/cursors/art`, because `kdos theme <accent>` regenerates the theme from it on the
running system; see
[How the theme is generated](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Flags
- `phosphor` — The accent the system copy is generated in. A user's own accent is applied by
  `kdos theme` to the copy in their home directory.
- `--src` — Recolour the port's own `art/` rather than the installed
  `/usr/share/kdos/cursors/art`, which this package is about to install.

## Notes
`build.sh` writes two more things than the theme: the vendored art under
`/usr/share/kdos/cursors/art`, and `/usr/share/icons/default/index.theme`, which inherits
`KDOS-cursors` and is the theme a program falls back to when it never sees `XCURSOR_THEME`: one
started through `sudo` or `pkexec`, or an X client; see the `XCURSOR_THEME` row in
[Starting a session](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#starting-a-session).

The package puts nothing in `/etc/skel`. The copy in `/etc/skel/.icons`, which a box needs because
it shares the home directory and not the host's `/usr/share/icons`, is written by `kdos theme` in
`70_image`'s `050_theme.sh`; a second copy owned by this package would be rewritten there and show
up as modified files in `kpkg check`.

The aliases point from the X11 names to the CSS names, never the other way. A tree laid out in the
opposite direction, installed over this one, would leave a link and its target pointing at each
other, and kpkg refuses a symbolic link loop.

`vendor.py` re-vendors `art/` from a Bibata release on a development host, with network. The build
never runs it; its output is committed.
