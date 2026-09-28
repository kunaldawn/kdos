# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The Python package carries version 0.0.0, which upstream's release script
# rewrites. FreeCAD reads ifcopenshell.version, and below 0.7 it offers to
# fetch IfcOpenShell over the network instead of using this one.
patch -p1 -i "$PORT_SRC/python-version.patch"

# SWIG 4.3 dropped the Python 2 compatibility macros (PyInt_*,
# SWIG_Python_str_FromChar) the typemaps call. SWIG also returns an
# OpaqueCoordinate through its value wrapper, which the unconstrained
# variadic constructor then takes as a coordinate component and fails to
# compile; the patch limits that constructor to number pointers.
patch -p1 -i "$PORT_SRC/swig-4.5.patch"

# VERSION_OVERRIDE versions the shared libraries from the VERSION file;
# without it their soname says 0.8.0. Only the three schemas in use (IFC2X3, IFC4 and
# IFC4X3_ADD2) are compiled: every other schema adds a generated source of
# several hundred thousand lines. CGAL is off because the svgfill module it
# brings needs the svgpp submodule, which the tag archive leaves empty; the
# OpenCASCADE kernel is the one geometry backend. OpenCOLLADA, USD and
# RocksDB are not ports; PROJ only adds Earth-centred glTF output and stays
# off. IfcGeomServer serves only Blender's add-on. Headers
# go under include/ifcopenshell, since upstream installs top-level
# directories named ifcparse, ifcgeom and serializers.
cmake -S cmake -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR:PATH=lib \
	-DCMAKE_INSTALL_INCLUDEDIR:PATH=include/ifcopenshell \
	-DBUILD_SHARED_LIBS=ON \
	-DVERSION_OVERRIDE=ON \
	-DADD_COMMIT_SHA=OFF \
	-DUSE_CCACHE=OFF \
	-DBUILD_ONLY_COMMON_SCHEMAS=ON \
	-DBUILD_IFCGEOM=ON \
	-DBUILD_IFCPYTHON=ON \
	-DBUILD_CONVERT=ON \
	-DBUILD_GEOMSERVER=OFF \
	-DBUILD_QTVIEWER=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DWITH_OPENCASCADE=ON \
	-DWITH_CGAL=OFF \
	-DCOLLADA_SUPPORT=OFF \
	-DGLTF_SUPPORT=ON \
	-DHDF5_SUPPORT=ON \
	-DIFCXML_SUPPORT=ON \
	-DUSD_SUPPORT=OFF \
	-DWITH_PROJ=OFF \
	-DWITH_ROCKSDB=OFF \
	-DUSE_MMAP=OFF \
	-DPYTHON_EXECUTABLE=/usr/bin/python3 \
	-DPython_EXECUTABLE=/usr/bin/python3
cmake --build build
DESTDIR=$PKG cmake --install build
