## About

R is the language and environment for statistics and data analysis: the interpreter, `Rscript`, the
base and recommended packages (Matrix, MASS, lattice, survival, nlme, mgcv and the rest), the
graphics devices, and `libR.so`, through which RKWard and Cantor embed R. Linear algebra runs on
[openblas](#port-openblas). The X11 graphics device and the `tcltk` package run under Xwayland, and
cairo and pango give the PNG, SVG and PDF devices anti-aliased text with no display at all.

## Flags

- `CXXFLAGS="$CXXFLAGS -D__MUSL__"` — Defines `__MUSL__` for C++. The flags are recorded in R's
  `Makeconf`, which every later package build reads, and Rcpp and the packages built on it choose
  their C library code paths by this macro.
- `--sysconfdir=/etc/R` — R's build reads no `sysconfdir`: its configuration files (`Renviron`,
  `Makeconf`, `repositories`) are written under `R_HOME/etc`, `/usr/lib/R/etc`, whatever this says.
- `rdocdir=/usr/share/doc/R`, `rincludedir=/usr/include/R`, `rsharedir=/usr/share/R` — Put R's
  documentation, C headers and shared data under `/usr/share` and `/usr/include` instead of inside
  `R_HOME`, where upstream's defaults put them.
- `--enable-R-shlib` — Builds `libR.so`, the shared library the `R` binary links and an embedding
  program loads. Upstream's default is off.
- `--disable-R-static-lib` — Builds no `libR.a`, upstream's default.
- `--disable-java` — Skips R's Java setup. rJava is not shipped, and on its default configure would
  probe a JDK and record its paths in `/usr/lib/R/etc/javaconf`.
- `--disable-nls` — Builds and installs no translated messages. KDOS ships English only (see
  [the size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `--enable-openmp` — Uses OpenMP where R's code supports it, upstream's default.
- `--with-recommended-packages` — Builds and installs the recommended packages, which ship inside
  the tarball under `src/library/Recommended`, so they work with no network; any other CRAN package
  is installed by the user.
- `--with-blas=-lopenblas`, `--with-lapack` — Takes BLAS and LAPACK from OpenBLAS, which installs
  both, so matrix work runs threaded and vectorised instead of on R's reference copy.
- `--with-readline`, `--with-pcre2`, `--with-ICU`, `--with-cairo`, `--with-libpng`,
  `--with-jpeglib`, `--with-libtiff`, `--with-libdeflate-compression` — Upstream's defaults, named:
  the console's line editing, regular expressions, ICU collation, the cairo-based bitmap and vector
  devices, the image formats `png()`, `jpeg()` and `tiff()` write, and libdeflate for compressing
  lazy-loaded R objects.
- `--with-x` — Builds the X11 graphics device and data editor, `R_X11.so`.
- `--with-tcltk`, `--with-tcl-config=/usr/lib/tclConfig.sh`,
  `--with-tk-config=/usr/lib/tkConfig.sh` — Builds the `tcltk` package against [tcl](#port-tcl) and
  [tk](#port-tk), naming their configuration scripts directly.
- `LIBR_LDFLAGS=` — Rewrites `libR.pc` with `install-pc` and this make variable emptied. The
  `install` target writes the build's `LDFLAGS` into the file's `Libs:` line, from which every
  program that links `libR` through pkg-config would take them.

## Notes

`make -C src/nmath/standalone shared` and its install add `libRmath.so`, R's distribution functions
as a plain C library that other programs link without embedding R; the static `libRmath.a` the
install may copy is deleted.

The `test -e` and `test -d` lines at the end turn a silently dropped feature into a failed build.
configure turns Tcl/Tk support off with only a warning when it cannot use tcl or tk, so a build
without `tcltk.so` otherwise installs cleanly; `libR.so`, `R_X11.so` and the six recommended
packages they name are checked with it, so the package never ships without any of them.
