## About

GObject Introspection describes a C library built on GObject in a form other languages can read.
`g-ir-scanner` reads a library's headers and its compiled code and writes a `.gir` file, an XML
description of the API; `g-ir-compiler` turns that into a binary `.typelib`, which a language
binding such as PyGObject or GJS loads at run time to call the library without generated glue
code. The port installs those tools, `libgirepository-1.0`, and the base GIR and typelib files for
libraries that do not ship their own, among them `cairo-1.0`, `fontconfig-2.0`, `freetype2-2.0`, `GL-1.0`,
`DBus-1.0`, `libxml2-2.0`, `Vulkan-1.0` and the X11 ones.

Every port that generates introspection data names it in `depends`, and with it
[glib-introspection](#port-glib-introspection), whose GLib, GObject and Gio GIRs every new GIR
includes. GLib itself is built without introspection, because this port needs GLib first; see
[A second build of the same source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source).

## Flags

- `-D build_introspection_data=true` — Build and install the base GIR and typelib files as well
  as the library and the tools. This is upstream's default.
- `-D cairo=disabled` — Do not use cairo in the tests. The option affects only the test suite,
  which is not built (`-D tests=false`); the `cairo-1.0` GIR is installed either way.
- `-D doctool=disabled` — Install no `g-ir-doc-tool`, which renders documentation from a GIR, and
  run none of its tests.
- `-D wrap_mode=nodownload` — Never download a meson subproject, so a missing dependency stops the
  configuration instead of being fetched.

## Notes

`g-ir-scanner` finds the shared library a GIR describes by running `ldd` on the program it
compiles and reading the output. musl installs no `ldd` command; [musl-ldd](#port-musl-ldd)
installs `/usr/bin/ldd`, a script that runs musl's dynamic loader with `--list`, which is why it is
a dependency. Without it the scanner step of every port that generates introspection data has no
`ldd` to run.
