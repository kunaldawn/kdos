## About

VTE is GNOME's terminal emulator widget: a GTK widget that runs a program on a pseudo-terminal and
draws its screen. This port builds it for both toolkits, `libvte-2.91.so` for
[gtk3](#port-gtk3) and `libvte-2.91-gtk4.so` for [gtk4](#port-gtk4). In the tree,
[geany](#port-geany)'s terminal pane, [qemu](#port-qemu)'s GTK display (built `--enable-vte`, for its text consoles),
[virt-manager](#port-virt-manager) and [remmina](#port-remmina) use it. KDOS's own terminal,
`kdos-term`, does not.

## Flags

- `--wrap-mode=nodownload` — Never download a meson subproject. The release tarball carries
  simdutf and fast_float under `subprojects/`, and they are built from those copies;
  [fmt](#port-fmt) comes from its port, for its headers only.
- `-Dgtk3=true`, `-Dgtk4=true` — Build both widgets from one source tree. Both are upstream's
  defaults.
- `-Dgir=true`, `-Dvapi=true` — Generate the introspection data and the Vala bindings for both,
  through which Python, JavaScript and Vala programs use the widget. Both are upstream's defaults.
- `-Dgnutls=true` — Encrypt the scrollback that VTE spills to disk, with
  [gnutls](#port-gnutls). Without it every terminal opens with the warning "GnuTLS not enabled;
  data will be written to disk unencrypted!".
- `-Dicu=true` — Support legacy character sets, other than UTF-8, through [icu](#port-icu).
- `-Dfribidi=true`, `-Da11y=true` — Bidirectional text through [fribidi](#port-fribidi), and the
  accessibility interface. Both are upstream's defaults.
- `-D_systemd=false` — Build without libsystemd. Upstream's default is on, and then VTE places
  each child program in a systemd scope of its own over D-Bus. KDOS has no systemd (see
  [No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-Dterminfo=false` — Install no terminfo entry of VTE's own; the option is experimental and off
  by default.
- `-Dapp=false` — Build no `vte-2.91` test terminal, which upstream builds by default.
- `-Dglade=false` — Install no catalogue file for the Glade interface designer, which is not a
  port.
- `-Ddbg=false` — No extra debugging code, upstream's default.

## Patches

- `fix-W_EXITCODE.patch` — Defines `W_EXITCODE(ret, sig)` as `((ret) << 8 | (sig))` in `src/widget.cc` when the C library does not. VTE uses it to report a child killed with `SIGKILL` when the widget is destroyed; musl's `<sys/wait.h>` has no such macro, and the file does not compile without it.
