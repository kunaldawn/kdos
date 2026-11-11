## About

xdg-desktop-portal-gtk is the GTK backend behind [xdg-desktop-portal](#port-xdg-desktop-portal).
On KDOS it serves three interfaces the Plasma backend leaves to it: Settings for the GNOME and GTK
keys, Print and Email for a caller that asks this backend by name. Every other interface is
[xdg-desktop-portal-kde](#port-xdg-desktop-portal-kde)'s.

## Flags

- `-Dsettings=enabled` — Serve the Settings portal: every `org.gnome.desktop.*` key from GSettings,
  among them the GTK theme, the icon and cursor themes, the fonts, `gtk-im-module` and the
  window-button layout in `org.gnome.desktop.wm.preferences`, and `org.gnome.fontconfig`. GTK 4 on
  Wayland reads every one of those settings from the portal whenever it answers, and none from
  `settings.ini`; a key no backend returns takes GTK's built-in fallback. GTK 3 on Wayland does the
  same wherever `GTK_USE_PORTAL` is `1`, which is every host login and every box, with two
  exceptions: `gtk-modules` and `gtk-fontconfig-timestamp` come from kde-gtk-config's
  `org.gtk.Settings`, which the host's [gtk3](#port-gtk3) is patched to open beside the portal (a
  box's own GTK 3 is not, and its filtered bus does not reach the service), and `gtk-im-module` is
  set to `wayland` by GTK 3 itself on a compositor offering `text-input-v3`, above whatever the
  portal returns. The Plasma backend serves
  only `org.kde.*` and `org.freedesktop.appearance`, so without this one a GTK 4 program takes
  `gtk-im-module` `simple` and gets no input method, draws `menu:close` as its header-bar buttons,
  and wears Adwaita's icons, cursor and font. `enabled` rather than `auto` makes a missing
  [gsettings-desktop-schemas](#port-gsettings-desktop-schemas) or [fontconfig](#port-fontconfig)
  fail the configure instead of building the backend without the interface.
- `-Dwallpaper=disabled`, `-Dappchooser=disabled`, `-Dlockdown=disabled` — The Plasma backend
  serves wallpaper and the application chooser, and lockdown has no settings source on KDOS.

## Notes

The values it serves are the GSettings that [kde-gtk-config](#port-kde-gtk-config)'s kded module
writes from the Plasma settings, through [dconf](#port-dconf), and it announces a changed key with
`SettingChanged`.

`kde-portals.conf`, which [plasma-workspace](#port-plasma-workspace) installs, names `kde;gtk;` for
Settings. Where both backends answer a key, as for `org.freedesktop.appearance`, the front end
takes the Plasma backend's, so the colour scheme and accent stay Plasma's. The portal file's
`UseIn=gnome` line is not consulted when a `-portals.conf` names the backend.

The install puts a systemd user unit under `/usr/lib/systemd`, which `build.sh` removes; the
backend is started by D-Bus activation from its `.service` file's `Exec=` line.
