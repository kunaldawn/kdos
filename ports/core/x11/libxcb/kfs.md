## About

libxcb is the C binding of the X protocol, generated at build time from the XML protocol
descriptions in [xcb-proto](#port-xcb-proto). Xlib is built on it, and so is every X client in the
tree, the X11 backends of the toolkits among them. It also installs one library per extension
(`libxcb-randr`, `libxcb-xkb`, `libxcb-shm` and the rest).

## Flags

- `--disable-devel-docs` — Leave out the doxygen manual of the API. With the switch on, the manual
  is built when doxygen and `dot` are both installed, so the package's contents would follow what
  the build root holds.
- `--without-doxygen` — Do not look for doxygen at all.

## Notes

The tutorial in `doc/tutorial` is installed under `/usr/share/doc/libxcb/tutorial` whatever the
switches say. kpkg's documentation prune removes its `index.html`, and the stylesheet beside it,
`xcb.css`, is what the package keeps of it (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
