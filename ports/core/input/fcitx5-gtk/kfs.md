## About

fcitx5-gtk is [fcitx5](#port-fcitx5)'s GTK side: an input-method module for GTK 3
(`/usr/lib/gtk-3.0/3.0.0/immodules/im-fcitx5.so`) and one for GTK 4
(`/usr/lib/gtk-4.0/4.0.0/immodules/libim-fcitx5.so`), both built on `libFcitx5GClient`, the D-Bus
client for the running fcitx5 that the package also installs. A GTK program that loads the module
speaks to fcitx5 over the session bus directly, without KWin's `text-input` between them, so the
module is what gives a GTK program on X11 (under Xwayland) an input method. `fcitx5-gtk3-immodule-probing`
and `fcitx5-gtk4-immodule-probing` print which module a GTK program would choose; `fcitx5-diagnose`
runs them.

Which module a GTK program chooses when `GTK_IM_MODULE` is unset differs between the two toolkits,
and the [Input methods](../../../../docs/kdos/03-architecture/session.md#input-methods) section
of the session chapter says which route each one takes.

## Flags

- `-D ENABLE_GTK2_IM_MODULE=OFF` — No GTK 2 module. Upstream builds one by default and then
  requires GTK 2, which is not a port.
- `-D ENABLE_GTK3_IM_MODULE=ON`, `-D ENABLE_GTK4_IM_MODULE=ON` — The two modules, named because
  each one is the reason for a [gtk3](#port-gtk3) or [gtk4](#port-gtk4) dependency. Both GTKs are
  built with their X11 backend, so both modules link [libX11](#port-libX11) as well.
- `-D ENABLE_GIR=OFF` — No `FcitxG-1.0` GObject introspection data for `libFcitx5GClient`.
  Nothing in the tree loads it, and building it would make gobject-introspection a dependency.

## Notes

GTK 3 loads a module that is not compiled into `libgtk-3` only through the list in
`/usr/lib/gtk-3.0/3.0.0/immodules.cache`, and [gtk3](#port-gtk3) is built with every one of its
own modules compiled in, so it installs no such list. `build.sh` writes it from the staged module
with `gtk-query-immodules-3.0`, with the staging directory on `LD_LIBRARY_PATH` so that the module's
own `libFcitx5GClient` resolves, and strips the staging prefix from the path the tool records. A
list that does not name `fcitx5` fails the build instead of shipping a module GTK 3 never loads.
This package owns the file; a second port with a loadable GTK 3 module would have to write the
list for both.

GTK 4 needs no such list: it loads every module in `immodules/` through GIO's extension points.

The source is a `.tar.zst`, which `kpkg` copies into the work directory whole instead of
unpacking; `build.sh` unpacks it, as the other fcitx5 recipes do.
