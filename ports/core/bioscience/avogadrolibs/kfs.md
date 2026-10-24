## About

Avogadro libraries are the core of the Avogadro 2 molecular editor: molecule and crystal data
structures, readers and writers for the chemistry file formats, an OpenGL renderer and the Qt 6
widgets and plugins that build, view and analyse a structure. In KDOS they are there for
[kalzium](#port-kalzium), whose molecule viewer is built on them. The editor application itself,
Avogadro 2, is a separate project and is not a port.

## Flags

- `-DBUILD_SHARED_LIBS=ON` — Shared libraries, upstream's default.
- `-DENABLE_RPATH=OFF`, `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Avogadro's `cmake/InstallLocation.cmake`
  sets its own install run path unless `ENABLE_RPATH` is off; with both, the libraries and plugins
  install with none, and the loader finds them in `/usr/lib`.
- `-DUSE_QT=ON`, `-DQT_VERSION=6`, `-DUSE_OPENGL=ON` — The Qt 6 widget, GUI and OpenGL rendering
  libraries and the plugins, on [qt6-qtbase](#port-qt6-qtbase), [glew](#port-glew) and
  [libglvnd](#port-libglvnd).
- `-DOpenGL_GL_PREFERENCE=GLVND` — Link OpenGL through libglvnd's `libOpenGL` rather than the
  legacy `libGL`.
- `-DUSE_HDF5=OFF`, `-DUSE_MMTF=OFF` — HDF5 storage of large data sets, and the MMTF
  macromolecular format. Both are off in upstream's default; [hdf5](#port-hdf5) is a port but not
  in `depends`, and no MMTF library is.
- `-DUSE_PYTHON=OFF` — The Python bindings of the library API, off in upstream's default.
- `-DUSE_SPGLIB=OFF`, `-DUSE_LIBMSYM=OFF`, `-DUSE_PLOTTER=OFF` — Space groups (spglib), symmetry
  detection (libmsym) and the spectra, conformer, PDF and XRD plots (JKQTPlotter); none of the
  three libraries is a port, so those plugins are not built.
- `-DUSE_LIBARCHIVE=OFF` — The plugin package installer, the one plugin that needs libarchive,
  which downloads plugin packages over the network at run time and unpacks them.
- `-DBUILD_GPL_PLUGINS=OFF` — Leave out the code under the GPL (the QTAIM analysis plugin, and
  the GPL parts of the force-field plugin), so the package stays under the BSD licence in
  `license`. Off is upstream's default.
- `-DUSE_EXTERNAL_NLOHMANN=ON`, `-DUSE_EXTERNAL_PUGIXML=ON`, `-DUSE_EXTERNAL_TOMLPLUSPLUS=ON` —
  Build against the [nlohmann-json](#port-nlohmann-json), [pugixml](#port-pugixml) and
  [tomlplusplus](#port-tomlplusplus) ports instead of the copies in the source tree.
- `-DPython3_EXECUTABLE=/usr/bin/python3` — The interpreter path is compiled into the GUI library
  as the one that runs the script plugins (input generators, charge models, script file formats),
  so it names the system interpreter rather than whatever CMake finds first.

## Notes

The data that the fragment, molecule, crystal and input-generator plugins install comes from four
more tarballs in `source`, all tagged with the library's version. The build looks for each as a
sibling directory of the source tree named exactly `molecules`, `crystals`, `fragments` and
`avogenerators`, and the first three are otherwise cloned from GitHub at build time, which has no
network and fails. The archives unpack with the version in the name, so `build.sh` first moves
each to the name the build expects. A missing `avogenerators` is only reported and skipped, so
the `test -e` line after the install stops the build unless the generators' `pyproject.toml` is
in the package.
