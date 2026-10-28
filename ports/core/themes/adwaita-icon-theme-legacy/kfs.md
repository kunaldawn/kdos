## About
AdwaitaLegacy is the full-colour icon set GNOME moved out of Adwaita. Adwaita keeps full-colour
icons for places, devices, MIME types and a handful of emblems and status icons, and draws
everything else as a symbolic icon; AdwaitaLegacy holds the full-colour set under the old
freedesktop names (actions, applications, categories, emblems, MIME types and more) as PNGs from
8 to 48 pixels. [adwaita-icon-theme](#port-adwaita-icon-theme)'s `index.theme` names it first in
`Inherits=`, so an application that looks up an old full-colour name in Adwaita (`document-open`,
`edit-copy`) finds it here rather than a blank, and a theme lookup does not report the inherited
theme as missing.

## Notes
The meson build refuses to configure without `gtk4-update-icon-cache` or `gtk-update-icon-cache`,
which is why [gtk3](#port-gtk3) is in `depends`; the install step that runs it is skipped under
`DESTDIR`, so the package ships no `icon-theme.cache`, and `kpkg` writes the theme's cache when the
package is installed.

Upstream installs its licence files into `/usr/share/licenses/adwaita-icon-theme/`, the directory
adwaita-icon-theme installs the same three files into, and two packages owning one path is a
conflict. The recipe moves them to `/usr/share/licenses/adwaita-icon-theme-legacy/` after the
install.
