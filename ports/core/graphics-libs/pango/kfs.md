## About

pango lays out and renders text: it splits a paragraph into runs by script, direction and font,
shapes each run with [harfbuzz](#port-harfbuzz), breaks lines, and draws the result through cairo.
GTK 3 and 4 and every program built on them lay out their text with it, and so does the compositor,
[kdos-comp](#port-kdos-comp), for its window titles. Besides `libpango` this build installs
`pangocairo`, `pangoft2`, `pangoxft`, the introspection data, and the `pango-view`, `pango-list`
and `pango-segmentation` tools with their manual pages.

## Flags

- `-Dintrospection=enabled` — Generates `Pango-1.0.gir` and its typelib. `Gtk-3.0.gir` and
  `Gtk-4.0.gir` include it, and PyGObject programs load Pango and PangoCairo through it. Its GIR
  includes HarfBuzz's, which is why harfbuzz is built with introspection too.
- `-Dxft=enabled` — Builds `pangoxft`, pango's Xft backend, against [libXft](#port-libXft). FLTK's
  hybrid Wayland and X11 build stops at configure without it.
- `-Dlibthai=enabled` — Finds the word boundaries in Thai text through [libthai](#port-libthai).
  Thai is written with no spaces between words, and without libthai a paragraph wraps in the middle
  of a word.
- `-Dcairo=enabled`, `-Dfontconfig=enabled`, `-Dfreetype=enabled` — `pangocairo`, font discovery
  through fontconfig, and `pangoft2`. On Linux none of the three changes what is built: `cairo` is
  `enabled` by upstream's default, fontconfig is required on every platform but Windows and macOS
  whatever the switch says, and pango's `meson.build` looks FreeType up as optional even when the
  switch is `enabled`. A missing FreeType therefore drops `pangoft2` and `pangoxft` without an
  error.
- `-Dsysprof=disabled` — Builds no Sysprof tracing marks; Sysprof is not a port. This is upstream's
  default, named here.
- `-Dbuild-testsuite=false`, `-Dbuild-examples=false` — Builds neither the test suite nor the
  example programs, both of which upstream builds by default.
- `-Dman-pages=true` — Builds the manual pages of the three tools with `rst2man`, from
  [python3-docutils](#port-python3-docutils). Upstream's default is off.
