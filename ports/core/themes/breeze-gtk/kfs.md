## About

Breeze GTK is the GTK half of the Breeze look: `Breeze` and `Breeze-Dark` themes under
`/usr/share/themes`, each with a GTK 3 and a GTK 4 stylesheet and a GTK 2 `gtkrc`, so a GTK
application draws with Breeze's widgets and colours inside the Plasma session.
[kde-gtk-config](#port-kde-gtk-config) selects the theme and follows the active Plasma colour
scheme by writing its colours into the person's GTK configuration as the named colours the
stylesheets define (`theme_bg_color_breeze` and the rest); the files this port installs are the
light and dark bases those colours override.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The port has no message catalogues, so the switch changes
  nothing here; it is passed as in every KDE recipe, so a release that adds catalogues still
  builds none (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The build renders each theme with `build_theme.sh`: `render_assets.py` draws the check boxes,
radio buttons, scroll-bar and other image assets with pycairo from the colours of
`BreezeLight.colors` and `BreezeDark.colors`, and `sassc` compiles the GTK 3 and GTK 4
stylesheets. The configure runs `python3 -c "import cairo"` and stops when it fails, so
[python3-cairo](#port-python3-cairo) is a build requirement, not an option; `sassc` is
[sassc](#port-sassc). The two colour schemes come from [breeze](#port-breeze), which the
configure finds as a CMake package (`BREEZE_COLOR_INSTALL_ROOT`); that puts Qt in this port's
closure, and with it the `43_toolkits` phase.

The `gtk-2.0` directories are installed whatever is present. No GTK 2 is ported, so they are
unused data, and the configure's search for GTK 2's pixmap engine reports it missing without
effect on the build.
