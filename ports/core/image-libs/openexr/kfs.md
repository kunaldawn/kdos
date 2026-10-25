## About

[OpenEXR]{.idx} is the high-dynamic-range image format of film and 3D rendering, as a library
(`libOpenEXR`, `libOpenEXRCore`, `libOpenEXRUtil`, `libIex`, `libIlmThread`) and a set of `exr*`
tools (`exrheader`, `exrinfo`, `exrcheck`, `exrmaketiled` and others). blender, gimp, krita,
darktable, imagemagick, opencv and libjxl link it. [imath](#port-imath) is its maths library.

## Flags

- `-DOPENEXR_BUILD_TOOLS=ON` — Build the `exr*` programs; upstream's default, named.
- `-DOPENEXR_BUILD_EXAMPLES=OFF` — Build and install no example programs, which upstream builds by
  default.
- `-DOPENEXR_INSTALL_DOCS=ON` — Generate and install a manual page for each tool, by running
  `help2man` on the built program.

## Notes

The build runs twice. The rules that generate the manual pages name each tool by its path in the
build tree, not as a CMake target, so nothing orders them after the programs they read. `build.sh`
therefore builds everything first, then reconfigures the same tree with `-DOPENEXR_INSTALL_DOCS=ON`
and builds again, when the programs already exist.

OpenJPH, the High-Throughput JPEG 2000 codec OpenEXR uses for its HTJ2K compression, is not a port.
When it is not found, OpenEXR compiles the copy it carries in `external/OpenJPH` into
`libOpenEXRCore`, and it does so here.
