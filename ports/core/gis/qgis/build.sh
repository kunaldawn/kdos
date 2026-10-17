#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/10-libspatialindex_2_1.patch"
patch -p1 -i "$PORT_SRC/20-qgstyle-infinite-loop.patch"
patch -p1 -i "$PORT_SRC/40-qt6.10.patch"
patch -p1 -i "$PORT_SRC/50-qstring.patch"
patch -p1 -i "$PORT_SRC/offline-defaults.patch"
patch -p1 -i "$PORT_SRC/sip-movable.patch"
patch -p1 -i "$PORT_SRC/system-meshoptimizer.patch"

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DQGIS_MANUAL_SUBDIR=share/man \
	-DBUILD_WITH_QT6=TRUE \
	-DENABLE_TESTS=FALSE \
	-DUSE_CCACHE=OFF \
	-DWITH_CORE=TRUE \
	-DWITH_GUI=TRUE \
	-DWITH_DESKTOP=TRUE \
	-DWITH_ANALYSIS=TRUE \
	-DWITH_AUTH=TRUE \
	-DWITH_OAUTH2_PLUGIN=TRUE \
	-DWITH_PYTHON=TRUE \
	-DWITH_BINDINGS=TRUE \
	-DBINDINGS_GLOBAL_INSTALL=TRUE \
	-DWITH_QSCIAPI=TRUE \
	-DWITH_QGIS_PROCESS=TRUE \
	-DWITH_3D=TRUE \
	-DWITH_QUICK=FALSE \
	-DWITH_SERVER=FALSE \
	-DWITH_SERVER_LANDINGPAGE_WEBAPP=FALSE \
	-DWITH_CRASH_HANDLER=FALSE \
	-DWITH_CUSTOM_WIDGETS=FALSE \
	-DWITH_GRASS7=FALSE \
	-DWITH_GRASS8=FALSE \
	-DWITH_POSTGRESQL=TRUE \
	-DWITH_SPATIALITE=TRUE \
	-DWITH_QSPATIALITE=TRUE \
	-DWITH_ORACLE=FALSE \
	-DWITH_HANA=FALSE \
	-DWITH_PDAL=TRUE \
	-DWITH_EPT=TRUE \
	-DWITH_COPC=TRUE \
	-DWITH_DRACO=TRUE \
	-DWITH_INTERNAL_LAZPERF=TRUE \
	-DWITH_INTERNAL_MDAL=TRUE \
	-DWITH_INTERNAL_POLY2TRI=TRUE \
	-DWITH_INTERNAL_MESHOPTIMIZER=FALSE \
	-DWITH_INTERNAL_NLOHMANN_JSON=FALSE \
	-DWITH_INTERNAL_SPATIALINDEX=FALSE \
	-DWITH_QTWEBKIT=FALSE \
	-DWITH_QTWEBENGINE=TRUE \
	-DWITH_QTSERIALPORT=TRUE \
	-DWITH_QTGAMEPAD=FALSE \
	-DWITH_PDF4QT=FALSE \
	-DWITH_GSL=TRUE \
	-DUSE_OPENCL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_OpenCL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Postgres=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_HDF5=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_NetCDF=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON \
	-Wno-dev
grep -q '^OPENCL_HPP_INCLUDE_DIR:PATH=/usr/include$' build/CMakeCache.txt
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/qgis"
test -x "$PKG/usr/bin/qgis_process"
ls "$PKG"/usr/lib/python3*/site-packages/qgis/_core*.so >/dev/null

install -Dm644 rpm/sources/qgis-mime.xml "$PKG/usr/share/mime/packages/qgis.xml"

cat > "$PKG/usr/share/applications/org.qgis.qgis.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=QGIS Desktop
GenericName=Geographic Information System
Comment=View, edit, analyse and print maps and geographic data
TryExec=qgis
Exec=qgis %F
Icon=qgis
Terminal=false
StartupNotify=false
StartupWMClass=org.qgis.qgis
Categories=Qt;Education;Science;Geography;
MimeType=application/x-qgis-project;application/x-qgis-project-container;application/x-qgis-layer-settings;application/x-qgis-layer-definition;application/x-qgis-composer-template;
Keywords=map;gis;geography;globe;shapefile;geopackage;postgis;wms;wfs;ogc;osgeo;qgis;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.qgis.qgis.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/qgis.png
