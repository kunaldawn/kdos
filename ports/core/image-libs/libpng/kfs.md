## About

libpng is the reference library for [PNG]{.idx} images, and the most depended-on image library in
the tree: freetype, cairo, harfbuzz, gdk-pixbuf, both Qt versions, the desktop's own programs
(`kdos-comp`, `kdos-shell`, `kdos-term`) and most of the graphical applications link it. It also installs
`pngfix`, which checks and repairs PNG files, and the `libpng-config` script.

## Flags

- `LIBS=-lpthread` — Add `-lpthread` to every link configure tests and the build makes. musl keeps
  its threads in libc, and its `libpthread.a` is an empty archive, so on this tree the flag links
  nothing and adds no dependency.

## Notes

The recipe keeps upstream's default of building the static archive as well, so `libpng16.a` (and the
`libpng.a` link to it) are installed beside the shared library.
