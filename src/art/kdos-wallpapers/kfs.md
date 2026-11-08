## About
`kdos-wallpapers` is the KDOS wallpaper in each of the eight accents, as Plasma wallpaper packages
under `/usr/share/wallpapers/KDOS-<Title>/`. Each package carries `contents/images_dark/` (the mark
on a dark ground: the accent's own `deep`, or the default accent's for `paper`) and
`contents/images/` (the same composition on the light scheme's ground), at 1920x1080, 2560x1600,
3840x2160 and 1080x1920. A package with both directories follows Plasma's light and dark switch by
itself, and Plasma picks the size nearest the screen and scales it to fill.

Nothing here is drawn by hand. `build.sh` runs [kdos-theme](#port-kdos-theme)'s `wallpaper`
generator over `mark.png`, a copy of the master artwork `kdos.png` at the top of the tree, and the
generator recolours the penguin's glow and wordmark into each accent; see
[How the theme is generated](../../../docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Notes
`mark.png` is a copy inside the port, not a path into the top of the tree: a port of KDOS's own is
rebuilt when a file under its own directory or under `src/libs` changes, and a source read from
anywhere else would let the master change under a package that never rebuilds. A new master is
copied here as well as to `kdos.png`.

The global themes name these packages as their default (`[Wallpaper] Image=KDOS-<Title>`), and that
default reaches only a desktop with no wallpaper set: Plasma's image wallpaper works its default
out once per desktop and never re-reads the global theme. An accent switch therefore sets the
wallpaper itself, and only on a desktop whose image is a `KDOS-*` package, so a picture the user
chose is never replaced; see
[kdos theme](../../../docs/kdos/04-programs/kdos-command.md#kdos-theme).

The package names are `theme_name` from libkcolor, so a renamed accent renames its wallpaper
package, and the global theme, the lock screen default and the accent switch all name it the same
way.
