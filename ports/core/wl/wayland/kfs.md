## About

Wayland is the display protocol of the KDOS desktop, and this port is its reference
implementation: `libwayland-client` and `libwayland-server`, the two halves every program on either
end of a Wayland socket links, `libwayland-cursor` and `libwayland-egl`, and `wayland-scanner`, the
code generator that turns a protocol's XML description into C stubs at another port's build time.
The compositor, [kdos-comp](#port-kdos-comp), and the [wlroots](#port-wlroots) library under it
are the server; the panel, the terminal, the resource monitor and the lock screen are clients, and
so is every toolkit's Wayland backend (GTK 3 and 4, Qt 5 and 6, SDL 3, GLFW), Mesa's Wayland
platform and [Xwayland](#port-xwayland). How the desktop is put together on top of it is in
[The session](../../../../docs/kdos/03-architecture/session.md).

The build is upstream's meson build with the documentation and the test suite left out, and with
the scanner's DTD check kept.

## Flags

- `-Ddocumentation=false` — Builds neither the HTML protocol book nor the generated `man3` pages
  for the library functions. The documentation build looks for `doxygen`, `dot`, `xsltproc`,
  `xmlto` and `mdbook` and stops configure when any is missing; no port provides `mdbook`, and
  `dot` comes from [graphviz](#port-graphviz), which `43_toolkits` builds after this port's phase.
- `-Ddtd_validation=true` — Compiles libxml2 into `wayland-scanner`, which then checks each
  protocol file it reads against the Wayland DTD it carries inside it. A file that fails prints a
  warning, and stops the scanner only when it is run with `--strict`. This is upstream's default;
  it is the reason the port depends on [libxml2](#port-libxml2).
- `-Dtests=false` — Builds no test programs; the suite is not run in the build.

## Notes

The scanner is a build tool as much as a library: every port that speaks a protocol beyond the
core one, from [wayland-protocols](#port-wayland-protocols) to the KDOS programs of `src/desktop`,
runs `wayland-scanner` during its own build to produce that protocol's C code, so it has to be
installed before any of them is configured.
