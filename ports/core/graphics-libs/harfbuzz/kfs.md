## About

HarfBuzz is the OpenType text shaping engine: given a run of Unicode text and a font, it chooses
the glyphs and their positions, applying ligatures, kerning, mark placement and the rules of
complex scripts such as Arabic, Devanagari and Thai. pango (and through it GTK and the compositor's
window titles), Qt, LibreOffice, Chromium, libass, SDL_ttf and FreeType's autohinter, which loads
it at run time, all shape through it. Besides `libharfbuzz` this build installs `libharfbuzz-icu`,
`libharfbuzz-gobject`, `libharfbuzz-cairo`, the subsetter, the raster, vector and GPU glyph
libraries, the `hb-view`, `hb-shape` and `hb-subset` tools, and the introspection data.

## Flags

- `-frandom-seed=harfbuzz` — With LTO, fixes the name GCC gives each object's `.gnu.lto_*`
  sections, which is otherwise random on every build. HarfBuzz is one of the hot libraries the tree
  builds with link-time optimisation, and every one of them carries the seed in `CFLAGS` and, for
  C++, `CXXFLAGS` (see [Every
  port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port)).
- `-D b_lto=true` — Link-time optimisation, through meson's own switch.
- `-D glib=enabled`, `-D gobject=enabled` — GLib's Unicode functions as a Unicode backend, and
  `libharfbuzz-gobject`, the GObject types introspection is generated from.
- `-D freetype=enabled` — The FreeType font functions (`hb-ft`), through which pango and FreeType's
  own users hand HarfBuzz their faces.
- `-D cairo=enabled` — Builds `libharfbuzz-cairo`, which draws shaped text with cairo, and the
  `hb-view` tool built on it.
- `-D png=enabled`, `-D zlib=enabled` — Let the raster glyph library write PNG and the vector one
  compress its output.
- `-D chafa=disabled` — Builds `hb-view` without chafa's terminal output. chafa reaches librsvg,
  which depends on harfbuzz, so linking it here would be a dependency cycle.
- `-D graphite2=enabled` — The Graphite shaper, for fonts that carry SIL Graphite tables, through
  [graphite2](#port-graphite2). Upstream's default is off.
- `-D icu=enabled` — Builds `libharfbuzz-icu`, the ICU Unicode functions as a separate module,
  which WebKitGTK requires.
- `-D gpu_demo=disabled` — Builds no `hb-gpu` demo. It needs GLEW, GLFW and GL, which are ports
  built in the same phase, so on `auto` the result would follow build order.
- `-D docs=disabled`, `-D tests=disabled`, `-D benchmark=disabled` — No API reference, no test
  suite and no benchmarks.
- `-D introspection=enabled` — Generates `HarfBuzz-0.0.gir` and its typelib. pango's GIR includes
  HarfBuzz's, so pango's introspection, and every binding built on it, needs this one.
