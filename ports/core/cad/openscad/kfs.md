## About
[OpenSCAD]{.idx} is a solid 3D modeller in which the model is a program: a script of primitives,
transformations and boolean operations, which OpenSCAD previews with OpenCSG and renders to a mesh
with the Manifold or CGAL geometry kernels, then exports as STL, 3MF, OFF, DXF or SVG. The port
builds the Qt 6 editor and viewer, `openscad`, from a commit of upstream's main branch, with the
three submodules it needs fetched as sources of their own.

## Flags
- `-DOPENSCAD_VERSION=$version` — the version the program reports. A release tarball carries it in
  `VERSION.txt`; a commit archive has none, and without the switch
  `cmake/Modules/openscad_version.cmake` takes the date of the build, so two builds of one recipe
  would report different versions.
- `-DOPENSCAD_COMMIT=${_commit:0:9}` — the commit shown beside the version. Upstream reads it from
  `COMMIT.txt` or from `git log`, and the unpacked archive has neither.
- `-DUSE_QT6=ON` — the interface on Qt 6, with `-DHEADLESS=OFF` keeping the GUI; both are
  upstream's defaults, named.
- `-DEXPERIMENTAL=OFF` — leave out the features upstream marks experimental; its default, named.
- `-DENABLE_TESTS=OFF` — skip the test suite, which upstream configures by default;
  `-DENABLE_GUI_TESTS=OFF` likewise leaves out the special test build of the GUI.
- `-DENABLE_PYTHON=OFF` — no embedded Python interpreter, an experimental feature that is off by
  default upstream.
- `-DOFFLINE_DOCS=OFF` — do not download the manual at build time, which the build, with no
  network, could not do. It is upstream's default, named.
- `-DUSE_CCACHE=OFF` — do not wrap the compiler in ccache, which upstream does by default when it
  finds it.
- `-DUSE_GLAD=ON` — load OpenGL entry points with GLAD, the loader in OpenSCAD's own tree, and with
  `-DUSE_GLEW=OFF` not with GLEW. Left unset, upstream picks GLAD unless the OpenCSG it finds is
  older than 1.6. An OpenCSG older than 1.6 links [glew](#port-glew) whatever the two switches
  say, because that OpenCSG itself loads OpenGL through GLEW; the [opencsg](#port-opencsg) port is
  newer, so the program carries GLAD alone.
- `-DENABLE_EGL=ON` — compile the EGL and, with `-DENABLE_GLX=ON`, the GLX ways of making an
  offscreen OpenGL context for command-line rendering. Each is used only when CMake finds it.
  Both are upstream's defaults.
- `-DENABLE_CGAL=ON` — build both geometry kernels, CGAL and, with `-DENABLE_MANIFOLD=ON`,
  Manifold. Both are upstream's defaults.
- `-DUSE_BUILTIN_MANIFOLD=OFF` — link the [manifold](#port-manifold) port instead of the
  `submodules/manifold` copy, and with `-DUSE_BUILTIN_CLIPPER2=OFF` the [clipper2](#port-clipper2)
  port instead of `submodules/Clipper2`. Upstream's default for both is the submodule, and the
  commit archive carries both submodule directories empty.
- `-DUSE_BUILTIN_OPENCSG=OFF` — link the [opencsg](#port-opencsg) port. It is upstream's default.
- `-DUSE_MIMALLOC=ON` — replace `malloc` with mimalloc, upstream's default. The mimalloc it uses is
  the submodule, which the recipe fetches at the commit upstream pins; it is built as a static
  library and overrides the allocator for the whole program.
- `-DENABLE_CAIRO=ON` — the PDF export, which draws through Cairo (`src/io/export_pdf.cc`).
  Upstream's default is `AUTO`, which uses Cairo only if it finds it; `ON` makes a missing
  [cairo](#port-cairo) a configure error.
- `-DENABLE_HIDAPI=ON` — the HIDAPI input driver, which talks to a 3D mouse directly rather than
  through a daemon, as a hard requirement instead of upstream's `AUTO`.
- `-DHIDAPI_INCLUDE_DIR="$(pkg-config --variable=includedir hidapi-hidraw)/hidapi"` — with
  `-DHIDAPI_LIBRARY=…`, point the HIDAPI driver at the hidraw backend. Upstream's
  `FindHidAPI.cmake` looks for the pkg-config names `hidapi` and `hidapi-libusb` and the libraries
  of those names, and the [hidapi](#port-hidapi) port builds only `libhidapi-hidraw`, so the finder
  alone does not find it.
- `-DENABLE_SPNAV=ON` — the libspnav input driver, for 3D mice through the `spacenavd` daemon,
  required instead of `AUTO`.
- `-DENABLE_QTDBUS=ON` — the D-Bus input driver, which upstream describes as "not for actual
  devices but for remote control": another program zooms, rotates and moves the view over D-Bus.
  It is upstream's default on a GUI build.
- `-DENABLE_GAMEPAD=OFF` — no gamepad input driver. It exists only on Qt 5 (`Qt5Gamepad`), and
  upstream stops the configure with an error if it is `ON` on Qt 6.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_Lib3MF=ON` — make a missing [lib3mf](#port-lib3mf) a configure error,
  so the 3MF import and export cannot drop out. It is upstream's default, named.

## Notes
The source is the archive of one commit (`_commit` in the recipe), not a release tarball, so
every git submodule is an empty directory in it. Three of them are needed and are not ports:
`sanitizers-cmake`, whose CMake module upstream loads with `find_package(Sanitizers)` on every
build, sanitising or not; `mimalloc`; and `libraries/MCAD`, the OpenSCAD part library installed for
scripts to `use`. Each is its own `source =` line, pinned to the submodule commit upstream records
(`_sancommit`, `_micommit`, `_mcadcommit`), and the first lines of `build.sh` replace the empty
directories with them. A version bump has to read the new commit's submodule pins and update all
three. The other submodules, Manifold, Clipper2 and OpenCSG, stay empty, because the switches above
take all three from ports.

Two things are removed from the installed resources. `locale` holds the translations, which
the tree's [language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
would remove in any case. `fonts/Liberation-2.00.1` is upstream's copy of the Liberation faces;
the [font-liberation](#port-font-liberation) port, in `depends`, installs them system-wide, and the
`10-liberation.conf` OpenSCAD keeps beside them still makes Liberation the default face for text in
a model.
