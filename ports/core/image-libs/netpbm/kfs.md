## About

[Netpbm]{.idx} is several hundred small converters and filters for the PBM, PGM, PPM and PAM image
formats (`pngtopnm`, `pnmscale`, `ppmtojpeg`, `pamcut` and so on) and the `libnetpbm` library they
share, built to be chained in a pipeline. groff and gts depend on it.

The source is a release of the `netpbm-make-dist` project, whose tarball carries Netpbm's HTML user
guide beside the code.

## Flags

Netpbm has no configure script that a recipe can drive; `build.sh` copies `config.mk.in` to
`config.mk` and passes every setting it changes on the `make` command line.

- `DEFAULT_TARGET=nonmerge` — Build each program as its own executable, rather than one merged
  binary that every program name links to. With `NETPBMLIBTYPE=unixshared`, `NETPBMLIBSUFFIX=so`,
  `STATICLIB_TOO=Y` and `LINKERISCOMPILER=Y`, this is `config.mk.in`'s own default, named so the
  build does not depend on it: `libnetpbm` is a Unix shared library, a static `libnetpbm.a` is built
  beside it, and linker options are passed in the `-Wl,` form a compiler driver takes.
- `CC=gcc` — Compile and link with `gcc`; the default is `cc`.
- `LDRELOC="ld --reloc"` — The command that combines relocatable objects into one; the default,
  `NONE`, says the system has none.
- `LINKER_CAN_DO_EXPLICIT_LIBRARY=Y` — Let the linker take a library as a file name on its command
  line, rather than only through `-l`.
- `CFLAGS_SHLIB=-fPIC` — Compile the library's objects as position-independent code; the default is
  empty.
- `STRIPFLAG=` — Install the programs unstripped. Netpbm's installer strips by default, and a port
  keeps an upstream's stripping switched off so the package carries the symbols its flags compiled;
  see [Size and language
  policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- `WANT_SSE=Y` — Use the SSE intrinsics where Netpbm's code has them.
- `TIFFLIB=-ltiff` — With `JPEGLIB=-ljpeg`, `PNGLIB=-lpng`, `ZLIB=-lz` and `JASPERLIB=-ljasper`,
  build the converters for TIFF, JPEG, PNG and JPEG 2000 against those libraries. The first four
  default to `NONE`, which leaves their converters out; JasPer defaults to the copy of libjasper in
  `converter/other/jpeg2000/libjasper`, so the setting links the [jasper](#port-jasper) port
  instead of that copy. The empty `*HDR_DIR` settings say the headers are in the compiler's default
  path.
- `X11LIB=NONE` — Build no `pamx`, the program that displays an image on an X server.
- `LINUXSVGALIB=NONE` — Build no `ppmsvgalib`, which needs Svgalib.
- `RGB_DB_PATH=/usr/share/netpbm/rgb.txt` — Look up colour names in the one colour database the
  package installs. The default list also names `/etc/X11/rgb.txt` and other X11 paths, and
  `/etc/X11` must never exist on KDOS; see [No Xorg
  server](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Notes

`make package` assembles the build in `$SRC_ROOT/netpbm-pkg`, and `build.sh` copies its parts into
`$PKG` by hand: `bin` and the libraries, the `sharedlink` and `staticlink` development links, the
headers, and `misc` (with `rgb.txt`) into `/usr/share/netpbm`. The package directory holds only a
`pkgconfig_template`, which Netpbm's interactive `installnetpbm` script fills in; `build.sh` does not
run that script, so it writes `netpbm.pc` itself.

The manual pages are generated from the HTML user guide with Netpbm's own `buildtools/makeman`,
which is why `python3` is a build dependency. Every page is converted except the guide's index and
error pages (`directory.html`, `error.html`, `liberror.html`, `libnetpbm_dir.html`) and the pages of
programs this build does not provide: `hpcdtoppm` (a dummy in the source tree, since its code is
distributed separately), `ppmsvgalib` (no Svgalib) and `vidtoppm`, which Netpbm never builds. The
converted index becomes `netpbm(1)`.

`JBIGLIB` is left at its default, so `jbigtopnm` and `pnmtojbig` are linked against the copy of
libjbig in `converter/other/jbig/libjbig`, not the [jbigkit](#port-jbigkit) port.
