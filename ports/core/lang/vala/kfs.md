## About

[Vala]{.idx} is a C#-like language that compiles to C on top of GObject. The port installs the
compiler, `valac`, the `vapigen` binding generator and the bundled `.vapi` bindings. GNOME libraries
generate their `.vapi` bindings with it (libadwaita, gtksourceview, vte, libsecret and libpeas with
`-Dvapi=true`, librsvg with `-Dvala=enabled`), and Vala applications such as dino, deja-dup and
font-manager are compiled with it.

## Flags

- `--disable-valadoc` — Build no `valadoc`, the documentation generator. It needs graphviz's
  `libgvc`, and configure stops when that is missing; [graphviz](#port-graphviz) is built in
  `43_toolkits`, after this port's phase.
