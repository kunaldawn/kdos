## About

OpenImageIO (OIIO) reads, writes and processes images for film and visual-effects pipelines: one
API over OpenEXR, TIFF, JPEG, PNG, JPEG XL, WebP, HEIF, JPEG 2000, GIF, camera raw, OpenVDB and
video frames through FFmpeg, with colour management from OpenColorIO, an image cache, a texture
system and `ImageBufAlgo` processing. It is Blender's image I/O, and it installs `oiiotool`,
`iinfo`, `maketx` and the other command-line tools, and the Python module.

## Flags

- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Installs the library, the tools and the Python module with no
  `RPATH`. Upstream's install `RPATH` is `$ORIGIN`, `$ORIGIN/lib` and `$ORIGIN/../lib`, with every
  directory outside the build tree that a linked library came from; every library they load is in
  `/usr/lib`, which the loader searches anyway, so the entries would add lookups and nothing else.
- `-DOIIO_BUILD_TESTS=OFF` — Builds no unit tests, which upstream builds by default.
- `-DOIIO_BUILD_TOOLS=ON` — Builds the command-line tools, upstream's default.
- `-DSTOP_ON_WARNING=OFF` — Treats compiler warnings as warnings. Upstream's default is off for a
  supported release and on for a development branch or a CI build.
- `-DUSE_CCACHE=OFF` — Does not compile through ccache, which upstream uses whenever it finds it.
- `-DINSTALL_FONTS=OFF` — Installs none of the Droid Sans, Droid Sans Mono and Droid Serif faces
  upstream bundles in `src/fonts`; upstream's default installs them.
- `-DINSTALL_DOCS=ON` — Installs the licence, the third-party notices and the change logs under
  `/usr/share/doc`, upstream's default. The manual pages are generated with txt2man, which is not a
  port, so none are made.
- `-DOpenImageIO_BUILD_MISSING_DEPS=` — Empty, upstream's default, so a missing dependency is a
  configure error or a feature left out, never a download.
- `-DROBINMAP_INCLUDE_DIR` — Points at robin-map, the header-only hash table OIIO compiles in and
  installs nothing of. It is the recipe's second `source =`, unpacked beside the main tree. OIIO
  marks robin-map to be built locally whenever it is missing, whatever `BUILD_MISSING_DEPS` says,
  so without this path its CMake would clone robin-map from GitHub.
- `-DUSE_EXTERNAL_PUGIXML=ON` — Links the [pugixml](#port-pugixml) port. Upstream's default
  compiles the copy of pugixml it carries in `src/include/OpenImageIO/detail/pugixml` into the
  library.
- `-DUSE_PYTHON=ON`, `-Dpybind11_DIR` — Builds the Python module, finding pybind11's CMake files
  through `python3 -m pybind11 --cmakedir`, from [python3-pybind11](#port-python3-pybind11).
- `-DUSE_QT=OFF` — Builds no `iv`, the Qt image viewer. Qt is built in a later phase,
  `43_toolkits`, after this port, and upstream uses it whenever it finds it, so a rebuild on a
  finished system would otherwise add `iv` and a Qt dependency the recipe does not name.
- `-DUSE_FFMPEG=ON`, `-DUSE_FREETYPE=ON`, `-DUSE_GIF=ON`, `-DUSE_JXL=ON`, `-DUSE_LIBHEIF=ON`,
  `-DUSE_LIBRAW=ON`, `-DUSE_OPENCOLORIO=ON`, `-DUSE_OPENJPEG=ON`, `-DUSE_OPENVDB=ON`,
  `-DUSE_TBB=ON`, `-DUSE_WEBP=ON` — The optional dependencies this build uses, each a port in
  `depends`: video frames, text drawing in `ImageBufAlgo::render_text`, GIF, JPEG XL, HEIF and
  AVIF, camera raw, colour management, JPEG 2000, OpenVDB volumes, Intel TBB threading and WebP.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_WebP=ON` — Makes CMake's `find_package(WebP)` required, so a
  missing libwebp stops configure instead of building without the WebP plugin.
- `-DUSE_PTEX=OFF`, `-DUSE_LIBUHDR=OFF`, `-DUSE_OPENJPH=OFF`, `-DUSE_R3DSDK=OFF`, `-DUSE_NUKE=OFF`
  — Ptex, Ultra HDR JPEG, OpenJPH (high-throughput JPEG 2000), the RED R3D SDK and Nuke's plugins.
  None of them is a port.
- `-DUSE_OPENCV=OFF` — Builds no OpenCV interop. [opencv](#port-opencv) is built in a later phase,
  `43_toolkits`, after this port.
- `-DUSE_DCMTK=OFF` — Builds no DICOM reader. [dcmtk](#port-dcmtk) is a port, installed in an
  earlier phase, so on the default the reader would be built whenever it is present; the switch
  leaves it out.
