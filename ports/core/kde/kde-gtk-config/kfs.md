## About

kde-gtk-config makes GTK applications follow Plasma's settings. Its kded module, `gtkconfig`,
watches the Plasma configuration and writes the theme, icon theme, cursor, fonts, colour scheme
and window-button layout to every place a GTK program reads them: `settings.ini` for GTK 3 and 4,
GSettings, and the XSETTINGS that [xsettingsd](#port-xsettingsd) serves to GTK programs under
Xwayland. It also renders CSS that gives [breeze-gtk](#port-breeze-gtk)'s client-side window
decorations Plasma's button set and colours.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The project ships no interface translations, so this changes
  nothing today; every KDE recipe passes it, so a catalogue added upstream is skipped as well (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_XSettingsd=ON` — Make `xsettingsd` a requirement of the
  configure. Upstream looks for the program only to report it; the module starts it, or sends it
  `SIGHUP`, whenever a setting changes, and without it a GTK program under Xwayland keeps the
  settings it started with.

## Patches

Each patch is offered upstream; the merge requests are to be opened against
`plasma/kde-gtk-config`, and each entry names its own once it exists.

`libadwaita-colors.patch` — libadwaita draws only from its own named colours and, from 1.6, from
CSS variables, never from the GTK theme, so the Breeze colour names alone leave a libadwaita
program in Adwaita's palette under every KDE scheme. The patch writes the accent, status, window,
view, header bar, sidebar, card, dialog, popover and thumbnail colours from the same KColorScheme
sets into the `colors.css` the kded module already maintains: as `@define-color` names for GTK 3
and 4, and for GTK 4 also as the `:root` custom properties libadwaita 1.10 reads. GTK 3 cannot
parse custom properties, so only `gtk-4.0/colors.css` carries them. KDOS writes nothing under
`~/.config/gtk-*`: this module is the one writer.

`gsd-xsettings-with-portal.patch` — The module serves `org.gtk.Settings` on every Wayland session;
upstream skips it when `GTK_USE_PORTAL` is `1`, which every KDOS login exports
(`/etc/profile.d/10-wayland.sh`) and kded inherits. GTK 3 on Wayland reads `gtk-modules` only from
that service's `Modules` property and ignores the line in `settings.ini`, so without it
`colorreload-gtk-module` never loads and a running GTK 3 program keeps its colours through every
colour-scheme change. The variable is about the GTK programs' dialogs, not about kded. The same
variable sends GTK 3 to the Settings portal for its other settings, and an unpatched GTK 3 then
never opens this service; [gtk3](#port-gtk3) carries the patch that opens it beside the portal, and
the two patches are needed together.

`first-login-config.patch` — At a new user's first login `~/.config/gtk-4.0` does not exist when
the module writes `gtk.css` and `colors.css`, and Qt's file open does not make a missing directory,
so a GTK 4 or libadwaita program wears stock Adwaita until the next colour change; the patch
creates each GTK config directory before writing into it. At the same login `kconf_update`'s
`gtk_theme` step writes `gtk-theme-name=Breeze` into `gtk-3.0/settings.ini` while kded is already
running and after the module has read that file, so unpatched the module takes the theme for
unset and removes Breeze's `window_decorations.css` and button assets; the patch takes the name from
GSettings, which holds the same value, whenever `settings.ini` names none. It is applied after
`libadwaita-colors.patch`, whose `modifyColorsCssFile` it touches.

## Notes

[sassc](#port-sassc) runs at build time: `kded/scss/build_decorations_css.sh` compiles
`window_decorations.scss` into the CSS installed as `/usr/share/themes/Breeze/window_decorations.css`,
beside [breeze-gtk](#port-breeze-gtk)'s theme. The configure stops without it.

GSettings keeps what the module writes only through [dconf](#port-dconf), GIO's persistent
backend; without it the values are held in memory by the writing process and no GTK 4 program
sees them. dconf and [breeze-gtk](#port-breeze-gtk), the theme the module selects by default, are
in `depends` for run time. [gsettings-desktop-schemas](#port-gsettings-desktop-schemas) is checked
at build time and read at run time: a key whose schema is missing is skipped.

The module is loaded by kded when the session starts and is the one writer of GTK settings; a GTK
setting changed by another tool is overwritten at the next Plasma change.
