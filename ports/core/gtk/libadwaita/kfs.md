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
- `-Dintrospection=enabled` — Generate `Adw-1.gir` and its typelib, upstream's `auto` named so
  that a missing [gobject-introspection](#port-gobject-introspection) is an error. A Python or
  JavaScript application uses libadwaita through the typelib.
- `-Dvapi=true` — Generate the Vala bindings, for which [vala](#port-vala) is in `depends`.
- `-Dexamples=false` — Build and install neither the demo application, `adwaita-1-demo`, nor the
  examples, which upstream does by default.
- `-Dprofiling=false` — No profiling instrumentation, upstream's default.
