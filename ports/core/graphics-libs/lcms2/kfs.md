## About

Little CMS is a colour-management engine: `liblcms2` transforms pixels between colour spaces by
applying ICC profiles, the files that describe how a camera, scanner, screen or printer renders
colour. It also installs the command-line tools `transicc`, `linkicc`, `psicc`, `jpgicc` and
`tificc`, the last two applying a profile to a whole JPEG or TIFF image. Many ports link it,
[ffmpeg](#port-ffmpeg), [imagemagick](#port-imagemagick) and [gst-plugins-bad](#port-gst-plugins-bad)
among them.

## Flags

- `--with-jpeg`, `--with-tiff`, `--with-zlib` — Build with [libjpeg-turbo](#port-libjpeg-turbo),
  [libtiff](#port-libtiff) and [zlib](#port-zlib). In lcms2's `configure` these only say "try":
  each library is probed by its headers and a test link, and a probe that fails sets the library
  aside with no error. `jpgicc` and `tificc` are then left out of the build, since each is built
  only under the matching `HasJPEG` or `HasTIFF` condition.

## Notes

Because the three switches cannot make `configure` fail, `build.sh` checks its result: after
`configure` it looks in the generated `Makefile` for `LIB_JPEG = -ljpeg`, `LIB_TIFF = -ltiff` and
`LIB_ZLIB = -lz`, the values each probe sets only on success, and stops the build naming whichever
is missing. A library that disappears from the build therefore fails the port instead of shipping
it without `jpgicc` or `tificc`.
