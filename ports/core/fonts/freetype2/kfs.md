## About
FreeType reads font files (TrueType, OpenType, Type 1, CFF, WOFF and WOFF2, PCF and BDF bitmap
fonts and more) and renders their glyphs into bitmaps, with hinting. Nearly every program that
draws text on KDOS reaches it, usually through [fontconfig](#port-fontconfig), cairo or
[harfbuzz](#port-harfbuzz), and the desktop's own programs (`kdos-shell`, `kdos-res`,
`kdos-term`, `kdos-lock`) draw their text through [fcft](#port-fcft), which loads faces with
FreeType.

## Flags
- `--enable-freetype-config` — Install the `freetype-config` script, which prints the compiler and
  linker flags for FreeType as `pkg-config freetype2` does. Off by upstream's default; on, a build
  system that asks the script rather than pkg-config still finds the library.
- `--with-zlib=yes` — Decompress gzip-compressed fonts (such as `.pcf.gz`) with the system
  [zlib](#port-zlib) rather than the copy FreeType carries. Named `yes`, a missing library is a
  configure error instead of a silent fallback.
- `--with-bzip2=yes` — Read bzip2-compressed fonts (`.pcf.bz2`), through [bzip2](#port-bzip2);
  required, as above.
- `--with-png=yes` — Decode the PNG glyph images of colour bitmap fonts (`CBDT` and `sbix` tables,
  as in [noto-emoji](#port-noto-emoji)), through [libpng](#port-libpng); without it those glyphs
  do not render.
- `--with-brotli=yes` — Decompress WOFF2 fonts, which are Brotli-compressed, through
  [brotli](#port-brotli).
- `--with-harfbuzz=dynamic` — The autohinter uses HarfBuzz to find the glyphs that OpenType
  features reach (small capitals, ligatures, alternates) and hint them like their base letters. With
  `dynamic`, FreeType does not link `libharfbuzz`; it calls `dlopen("libharfbuzz.so.0")` at run time
  and does without it when the library is absent. HarfBuzz is built against FreeType, so linking it
  here would be a dependency cycle.
- `--with-librsvg=no` — Do not look for librsvg. Upstream uses it only to render OpenType SVG
  glyphs in FreeType's demo programs, which this port does not build, so the library is the same
  either way.

## Notes
Because HarfBuzz is reached only through `dlopen`, `readelf -d /usr/lib/libfreetype.so` does not
list it, and no dependency records it: the autohinter's OpenType support is there exactly when the
[harfbuzz](#port-harfbuzz) package is installed, and FreeType falls back to hinting without it,
with no error, when `libharfbuzz.so.0` cannot be loaded.
