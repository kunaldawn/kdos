## About

giflib reads and writes [GIF]{.idx} images. `libgif` is linked by libwebp's `gif2webp`, libjxl,
imagemagick, leptonica, gdal, krita and others, and the package also installs five small utilities:
`gifbuild`, `giffix`, `giftext`, `giftool` and `gifclrmp`.

## Notes

giflib has no configure script; the recipe names make targets instead of running `make` and `make
install`. Upstream's `all` target, and with it `install`, also runs `make -C doc`, which builds the
HTML documentation and the manual pages with `xmlto`. `build.sh` builds `shared-lib` and
`static-lib`, and the install targets it names build only the utilities on top (`install-bin`
depends on them); `install-man` copies the pages for those five and `giflib(7)`, which the release
already carries in `doc/`. Nothing is built from the XML sources, so `xmlto` is not a dependency.

The utilities are linked against `libgif.a`, upstream's choice, which is why the static library is
built, and `install-lib` installs it beside the shared one.
