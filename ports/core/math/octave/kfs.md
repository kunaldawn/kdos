## About
[GNU Octave]{.idx} is a numerical computing language largely compatible with MATLAB, with an
interpreter for the terminal and a Qt 6 desktop (`octave --gui`) that has a QScintilla editor,
a variable browser and plot windows. Figures are drawn by its `qt` graphics toolkit, an OpenGL
widget, and printed to PDF, EPS and SVG through [gl2ps](#port-gl2ps); [gnuplot](#port-gnuplot) is
the second graphics toolkit, run as a separate program.

## Flags
- `--enable-shared` — build Octave's own libraries (`liboctave`, `liboctinterp`, `liboctgui`)
  shared, which is upstream's default; compiled `.oct` extensions link them.
- `--disable-rpath` — write no run path into the libraries and programs; everything is found in
  `/usr/lib`.
- `--disable-java` — leave out the Java interface (`javaObject`, `javaMethod` and the rest), so the
  build does not look for a JDK.
- `--enable-openmp` — with `--enable-fftw-threads`, `--enable-rapidjson` and `--enable-readline`:
  OpenMP threading, multi-threaded FFTW, `jsondecode`/`jsonencode` through
  [rapidjson](#port-rapidjson), and command-line editing through [readline](#port-readline). Each
  is upstream's default. The switch only lets configure look; a missing library is still a warning,
  and the `config.h` check in the Notes is what stops the build.
- `--with-qt=6` — build the GUI against Qt 6.
- `--with-blas=-lopenblas` — with `--with-lapack=-lopenblas`: take BLAS and LAPACK from
  [openblas](#port-openblas), which provides both, instead of letting configure search for a
  library.
- `--with-magick=GraphicsMagick++` — `imread`, `imwrite` and `imfinfo` through
  [graphicsmagick](#port-graphicsmagick)'s C++ library. It is upstream's default, named.
- `--with-openssl` — use libcrypto's MD5 and SHA routines for Octave's `hash` function.
- `--without-fltk` — no FLTK graphics toolkit; the Qt toolkit takes its place.
- `--without-x` — with `--with-wayland-client`: Octave asks the display for the screen's size and
  resolution (the root figure's `screensize` and `screenpixelsperinch`) through the Wayland client
  library instead of Xlib, so Octave's own code links no Xlib.

## Patches
- `abs.patch` — adds `abs` overloads for the unsigned and narrow integer types (`unsigned int`, `unsigned long`, `unsigned long long`, `unsigned short`, `unsigned char`, `signed char` and `short`) to `libinterp/corefcn/pr-output.cc`, ahead of the template that prints integer arrays and calls an unqualified `abs` on each element's value. The patch is Alpine's, which describes it only as needed on Alpine.

## Notes
Octave's configure answers a missing optional library with a warning and builds without the
functions it backs, so after the configure `build.sh` checks `config.h` for every one this port
depends on and stops on the first that is missing. The list covers
[suitesparse](#port-suitesparse) (UMFPACK, CHOLMOD, CXSparse, KLU, SPQR, for sparse algebra),
[arpack-ng](#port-arpack-ng) (`eigs`), [qrupdate](#port-qrupdate), [fftw](#port-fftw) in both
precisions, [glpk](#port-glpk), [qhull](#port-qhull) (`convhulln`, `delaunayn`),
[sundials](#port-sundials) with KLU (`ode15s`, `ode15i`), [hdf5](#port-hdf5) (`save -hdf5`),
GraphicsMagick, [libsndfile](#port-libsndfile) and [portaudio](#port-portaudio) (`audioread`,
`audioplayer`), [curl](#port-curl) (`urlread`, `webread`), RapidJSON, QScintilla, gl2ps, Qt 6,
OpenGL, FreeType, Fontconfig and readline. A dependency added to `depends` without a line in that
loop can be dropped by the configure unnoticed.

The tarball carries the manual already built, as Info, HTML, PDF and a Qt help collection, and it
is installed as it is; `build.sh` checks that `octave.info` and the `octave-gui` program were
installed. It removes the test files under `/usr/share/octave/<version>/etc/tests`.

The desktop entry is KDOS's own, written over upstream's for `StartupWMClass=org.octave.Octave`:
the GUI sets that as its desktop file name, and so as its Wayland app id.
