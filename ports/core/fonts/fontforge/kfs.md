## About

[FontForge]{.idx} is an outline and bitmap font editor. The port builds its GTK 3 editor window,
the `fontforge` command, which also runs FontForge's native scripts (`fontforge -script`) and
Python scripts without a window, and the `fontforge` Python module. The scripting half is what
the font ports use at build time: [ttf-dejavu](#port-ttf-dejavu) and
[terminus-ttf](#port-terminus-ttf) compile their TrueType files from their drawing sources and
bitmaps through it, and [font-liberation](#port-font-liberation)'s `make ttf-dir` builds its
faces with it.

## Flags

- `-DENABLE_GUI=ON` — Build the editor window, which is GTK 3 with gtkmm
  ([gtk3](#port-gtk3), [gtkmm3](#port-gtkmm3)). FontForge allows GDK only its X11 backend
  (`gdk_set_allowed_backends("win32,quartz,x11")` on Linux), so the window is an Xwayland client
  and depends on gtk3's X11 backend.
- `-DENABLE_NATIVE_SCRIPTING=ON`, `-DENABLE_PYTHON_SCRIPTING=ON` — FontForge's own scripting
  language and its embedded Python interpreter.
- `-DENABLE_PYTHON_EXTENSION=ON` — Build the `fontforge` and `psMat` modules for the system
  [python3](#port-python3), which the font ports import. Named `ON`, a build that cannot make them
  stops instead of leaving them out.
- `-DENABLE_LIBGIF=ON`, `-DENABLE_LIBJPEG=ON`, `-DENABLE_LIBPNG=ON`, `-DENABLE_LIBTIFF=ON`,
  `-DENABLE_LIBREADLINE=ON`, `-DENABLE_LIBSPIRO=ON`, `-DENABLE_WOFF2=ON`, `-DENABLE_HARFBUZZ=ON` —
  Each optional library named `ON`. Upstream defaults most of them to `AUTO`, which builds a
  narrower FontForge without a word when a library is missing; `ON` makes the search `REQUIRED`. The image
  libraries read background and glyph images, readline gives the script prompt line editing,
  [libspiro](#port-libspiro) adds Spiro curves, [woff2](#port-woff2) reads and writes WOFF2 faces,
  and [harfbuzz](#port-harfbuzz) shapes text in the editor's metrics view.
- `-DENABLE_DOCS=OFF` — Build no HTML manual, which needs Sphinx. The manual pages are installed
  either way.

## Notes

`build.sh` replaces upstream's desktop entry with its own. Upstream's claims TrueType and OpenType
files, which [font-manager](#port-font-manager)'s viewer claims as well; the replacement lists
FontForge's own `.sfd` and the Type 1, BDF, PCF, TeX and WOFF formats, so a double-clicked `.ttf`
or `.otf` opens in the viewer rather than the editor. `StartupWMClass=fontforge` matches the
window under Xwayland: the compositor takes the X window's `WM_CLASS` instance as its app id, and
GDK sets that to the program name, `fontforge`.
