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

libadwaita draws only from its own named colours and, from 1.6, from CSS variables, never from the
GTK theme, so the Breeze colour names alone leave a libadwaita program in Adwaita's palette under
every KDE scheme. `libadwaita-colors.patch` writes the accent, status, window, view, header bar,
sidebar, card, dialog, popover and thumbnail colours from the same KColorScheme sets into the
`colors.css` the kded module already maintains: as `@define-color` names for GTK 3 and 4, and for
GTK 4 also as the `:root` custom properties libadwaita 1.10 reads. GTK 3 cannot parse custom
properties, so only `gtk-4.0/colors.css` carries them. KDOS writes nothing under `~/.config/gtk-*`:
this module is the one writer. The patch is proposed upstream.
