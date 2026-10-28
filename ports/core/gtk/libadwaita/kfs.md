## About

libadwaita is GNOME's library of building blocks for [gtk4](#port-gtk4) applications: the
Adwaita stylesheet, adaptive widgets that rearrange for a narrow window (split views, navigation
views, view switchers), preference windows, toasts and dialogs, and `AdwStyleManager`, which
follows the desktop's light or dark preference. Many of the tree's GTK 4 applications are built
on it.

## Flags

- `--wrap-mode=nodownload` — Never download a meson subproject. The source carries wrap files for
  GTK, sassc and gi-docgen, and a copy of ministream under `subprojects/`, which is built from that
  copy. With downloads forbidden, a missing [gtk4](#port-gtk4) stops the configure instead of
  being answered by a GTK cloned and built inside the port.
- `-Dintrospection=enabled` — Generate `Adw-1.gir` and its typelib. Upstream's default is `auto`;
  named `enabled`, a missing [gobject-introspection](#port-gobject-introspection) is an error
  instead of a library without the data. A Python or JavaScript application uses libadwaita
  through the typelib.
- `-Dvapi=true` — Generate the Vala bindings, for which [vala](#port-vala) is in `depends`.
- `-Dexamples=false` — Build neither the demo application, `adwaita-1-demo`, which upstream
  installs, nor the `hello` example, which it builds without installing. Both are on by default.
- `-Dprofiling=false` — No `-pg` profiling instrumentation, upstream's default.
