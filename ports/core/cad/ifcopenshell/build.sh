# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/python-version.patch"

patch -p1 -i "$PORT_SRC/swig-4.5.patch"

patch -p1 -i "$PORT_SRC/boost-optional.patch"

patch -p1 -i "$PORT_SRC/hdf5-shared.patch"

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
