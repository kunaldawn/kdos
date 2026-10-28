## About
[FLTK]{.idx}, the Fast Light Toolkit, is a small C++ GUI library, with FLUID, its interface
designer, which writes the C++ code for a window laid out in it, and `fltk-options`, which sets
FLTK's system and user options. In KDOS it draws fldigi, flrig and Gmsh, among others. It is built as one hybrid
library: a program uses Wayland when a Wayland display is reachable and X11 otherwise, and
`FLTK_BACKEND=x11` in the environment sends it to Xwayland.

## Flags
- `CFLAGS=` — with `CXXFLAGS=` in front of `cmake`, and the tree's flags handed over in
  `-DCMAKE_C_FLAGS_RELEASE` and `-DCMAKE_CXX_FLAGS_RELEASE` instead. FLTK compiles with both, so
  the library is built with the tree's flags and `-O3`. What changes is `fltk-config`: it records
  `CMAKE_C_FLAGS`, which CMake fills from the environment's `CFLAGS`, as the `--cflags` and the
  `--cxxflags` it hands to every program built against FLTK, and it leaves the Release flags out.
  With the environment empty, those outputs carry FLTK's own include and feature flags and none
  of this build's, so a C-only flag such as `-std=gnu11` does not reach a C++ consumer's compile
  line.
- `-DFLTK_BACKEND_WAYLAND=ON` — with `-DFLTK_BACKEND_X11=ON`: the hybrid library. The Wayland
  backend draws with [cairo](#port-cairo) and lays out text with [pango](#port-pango); a hybrid
  build draws its X11 windows the same way, so the X11 half needs pango's `pangoxft` as well.
- `-DFLTK_USE_SYSTEM_LIBDECOR=ON` — use the [libdecor](#port-libdecor) port instead of compiling
  the copy FLTK carries. Window frames on Wayland come from the plugins that port installs.
  configure falls back to the bundled copy without failing when the port is older than 0.2.0 or
  its `libdecor/plugins-1` directory is missing.
- `-DFLTK_USE_DBUS=ON` — use D-Bus, which FLTK's Wayland backend needs to read the desktop's
  cursor theme.
- `-DFLTK_BUILD_SHARED_LIBS=ON` — build the shared libraries, which a program links by default,
  as well as the static archives FLTK always builds. Off by upstream's default.
- `-DFLTK_BUILD_GL=ON` — build `libfltk_gl`: OpenGL on EGL under Wayland and on GLX under X11.
  configure needs `gl.pc`, `egl.pc`, `glu.pc` and `wayland-egl.pc` for it.
- `-DFLTK_OPTION_CAIRO_WINDOW=ON` — with `-DFLTK_OPTION_CAIRO_EXT=ON`: build `Fl_Cairo_Window`,
  a window that sets up a Cairo context for the application's own drawing callback, and the
  "extended" support that provides a Cairo context in every `Fl_Window` subclass. FLTK's own
  drawing does not depend on either; both are off by upstream's default.
- `-DFLTK_USE_SYSTEM_LIBJPEG=ON` — with `-DFLTK_USE_SYSTEM_LIBPNG=ON` and
  `-DFLTK_USE_SYSTEM_ZLIB=ON`: link [libjpeg-turbo](#port-libjpeg-turbo), [libpng](#port-libpng)
  and [zlib](#port-zlib) instead of FLTK's bundled copies. Upstream's default on Linux, named.
- `-DFLTK_BUILD_FORMS=ON` — with `-DFLTK_BUILD_FLUID=ON`, `-DFLTK_BUILD_FLTK_OPTIONS=ON`,
  `-DFLTK_OPTION_SVG=ON`, `-DFLTK_OPTION_PRINT_SUPPORT=ON`, `-DFLTK_OPTION_FILESYSTEM_SUPPORT=ON`,
  `-DFLTK_OPTION_LARGE_FILE=ON` and `-DFLTK_USE_PTHREADS=ON`: the XForms compatibility library,
  FLUID, `fltk-options`, SVG images, printing, the file chooser's file system support, large
  files and threads. Each is upstream's default, named.
- `-DFLTK_BUILD_TEST=OFF` — with `-DFLTK_BUILD_EXAMPLES=OFF`, `-DFLTK_BUILD_HTML_DOCS=OFF`,
  `-DFLTK_BUILD_PDF_DOCS=OFF` and `-DFLTK_BUILD_FLUID_DOCS=OFF`: no demo programs, examples or
  Doxygen manuals.

## Notes
A hybrid build requires Xfixes, Xrender, Xft, Xcursor and Xinerama: configure warns about each
one whose headers are missing and then stops, rather than switching a feature off. Each library is
a port in `depends`.

`build.sh` writes its own desktop entries for FLUID and `fltk-options` in place of upstream's,
adding a `GenericName`, `Keywords` and a `StartupWMClass` to each, and naming the second
"FLTK Options". `Fl_Window::show(argc, argv)` sets the window
class, and so the Wayland app_id, from the program's file name, which is what `StartupWMClass`
names. The icons and the MIME types the entries name are upstream's, installed by `ninja install`.
