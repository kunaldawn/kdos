## About
AdwaitaLegacy is the full-colour icon set GNOME split out of Adwaita, which since 46 draws
symbolic icons only. [adwaita-icon-theme](#port-adwaita-icon-theme)'s `index.theme` names it
first in `Inherits=`, so a Qt or KDE application that looks up an old full-colour name in Adwaita
(`document-open`, `edit-copy`, a MIME-type icon) finds it here rather than a blank, and a theme
lookup does not report the inherited theme as missing.

## Notes
Upstream installs its licence files into `/usr/share/licenses/adwaita-icon-theme/`, the directory
adwaita-icon-theme installs the same three files into, and two packages owning one path is a
conflict. The recipe moves them to `/usr/share/licenses/adwaita-icon-theme-legacy/` after the
install.
