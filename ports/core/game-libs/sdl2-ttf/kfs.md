## About

SDL2_ttf renders TrueType and OpenType text into SDL 2 surfaces. It rasterises glyphs with
[freetype2](#port-freetype2) and, in this build, shapes each run of text with
[harfbuzz](#port-harfbuzz), so ligatures, combining marks and right-to-left and complex scripts
come out as the font intends. It is built against [sdl2-compat](#port-sdl2-compat), so an SDL 2
program using it runs on [sdl3](#port-sdl3).

## Flags

- `-DSDL2TTF_VENDORED=OFF` — Builds against the root's FreeType and HarfBuzz instead of the
  complete copies of both that the release tarball carries under `external/`. This is upstream's
  default everywhere but MSVC; named here, it keeps a second FreeType and HarfBuzz out of the
  library.
- `-DSDL2TTF_HARFBUZZ=ON` — Shapes text with HarfBuzz. Upstream's default is off, and without it
  SDL2_ttf places one glyph per character from the font's character map and kerning alone.
- `-DSDL2TTF_SAMPLES=OFF` — Builds none of the sample programs, which upstream builds by default
  when SDL2_ttf is the top-level project.
