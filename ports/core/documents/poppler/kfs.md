## About

Poppler is the PDF rendering library descended from Xpdf, with the `pdftotext`, `pdftoppm`,
`pdfimages`, `pdfinfo`, `pdfsig` and other command-line utilities. Many programs on KDOS read a
PDF through its bindings: the Qt 6 binding is the PDF backend of
[okular](#port-okular), [texstudio](#port-texstudio), [gimagereader](#port-gimagereader),
[labplot](#port-labplot), [cantor](#port-cantor) and [krita](#port-krita)'s PDF import; the GLib
binding is how [timg](#port-timg) shows a PDF in a terminal; its introspection data is what a
PyGObject program imports as `Poppler`. The utilities matter on their own:
[recoll](#port-recoll) indexes a PDF by running `pdftotext`, and without it reports success over a
directory of PDFs while indexing none of them.

## Flags

- `-DENABLE_UTILS=ON` — Build and install the command-line utilities, `pdftotext` among them.
- `-DENABLE_CPP=ON` — The C++ binding, `poppler-cpp`.
- `-DENABLE_GLIB=ON`, `-DENABLE_GOBJECT_INTROSPECTION=ON` — The GLib binding, which needs only
  [glib](#port-glib) and [cairo](#port-cairo) (GTK is looked for only to build a demo), and the
  `Poppler-0.18` GIR and typelib generated for it.
- `-DENABLE_GTK_DOC=OFF` — No gtk-doc reference for the GLib binding.
- `-DENABLE_QT6=ON`, `-DENABLE_QT5=OFF` — The Qt 6 binding only; nothing in the tree builds
  poppler against Qt 5.
- `-DBUILD_CPP_TESTS=OFF`, `-DBUILD_MANUAL_TESTS=OFF`, `-DBUILD_GTK_TESTS=OFF`,
  `-DBUILD_QT6_TESTS=OFF` — Poppler's test programs are gated by these, one per binding, not by
  `BUILD_TESTING`, and every one defaults to on.
- `-DENABLE_UNSTABLE_API_ABI_HEADERS=ON` — Install the private Xpdf-derived headers
  (`/usr/include/poppler/*.h`), which [gdal](#port-gdal)'s PDF driver compiles against. Upstream
  gives them no stability promise.
- `-DENABLE_BOOST=ON` — Use [boost](#port-boost)'s headers for the small containers in the Splash
  rasteriser; nothing links boost at run time.
- `-DENABLE_GPGME=ON`, `-DENABLE_NSS3=OFF` — Signature verification and signing through the GPG
  backend, [gpgmepp](#port-gpgmepp), as the only backend. With NSS also enabled, poppler makes NSS,
  the first backend it adds, the default for `pdfsig` and for every reader that signs.
- `-DENABLE_LIBCURL=OFF` — Leave out poppler's libcurl-based loader for documents fetched over
  HTTP.
- `-DENABLE_LCMS=ON`, `-DENABLE_LIBJPEG=ON`, `-DENABLE_LIBTIFF=ON`, `-DENABLE_HARFBUZZ=ON` — Colour
  management through [lcms2](#port-lcms2), DCT (JPEG) streams through
  [libjpeg-turbo](#port-libjpeg-turbo), TIFF output from `pdfimages` and `pdftocairo`, and font
  subsetting through [harfbuzz](#port-harfbuzz). With each option on, a missing library fails
  the configure.
- `-DENABLE_LIBOPENJPEG=openjpeg2` — JPEG 2000 (JPX) streams through
  [openjpeg](#port-openjpeg). The option is a boolean; `openjpeg2` is not one of CMake's false
  constants, so CMake reads it as on.
- `-DWITH_Cairo=ON`, `-DWITH_PNG=ON` — Let poppler search for cairo and libpng, the defaults of
  its optional package searches; the cairo output backend (`pdftocairo`, and the GLib binding)
  and PNG output need them.

## Notes

The three checks after the install stop the build unless `poppler-glib.pc`, the
`Poppler-0.18.typelib` and `poppler-qt6.pc` are in the package. Poppler's CMake turns the GLib
binding and its introspection data off without an error when cairo, GLib or
gobject-introspection is not found, so the first two lines make a missing one a failed build
instead of a reader that cannot open a PDF. A missing Qt 6 already stops the configure under
`-DENABLE_QT6=ON`; the third line checks that the binding reached the package.

Because the unstable headers carry no promise, a version bump of poppler has to be test-built
with [gdal](#port-gdal) as well: its PDF driver may fail to compile against the new headers.
