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

# Alpine's patches: accept libspatialindex 2.1 (its fix is in the release this
# tree carries), an infinite loop in the style database upgrade, the private
# Qt Sql headers the SpatiaLite driver needs from Qt 6.10 on, and a QString
# argument that no longer converts from an enum. offline-defaults.patch sets
# the global defaults so that a machine with no network is not contacted at
# every start: the version check, the welcome page's news feed and the
# plugin repository check are off.
patch -p1 -i "$PORT_SRC/10-libspatialindex_2_1.patch"
patch -p1 -i "$PORT_SRC/20-qgstyle-infinite-loop.patch"
patch -p1 -i "$PORT_SRC/40-qt6.10.patch"
patch -p1 -i "$PORT_SRC/50-qstring.patch"
patch -p1 -i "$PORT_SRC/offline-defaults.patch"

# QGIS Desktop on Qt 6 with PyQGIS (the Python console, Processing, DB
# Manager, MetaSearch and the plugin manager), qgis_process, 3D map views
# through Qt 3D, the SpatiaLite and PostgreSQL providers, EPT and COPC point
# clouds (the bundled laz-perf), the PDAL provider for LAS, LAZ and E57 point
# clouds, Draco-compressed 3D tiles, meshes (the bundled MDAL, with HDF5,
# NetCDF and XML formats), OpenCL acceleration through the ICD loader, GPS
# over serial ports, and QtWebEngine for HTML in layouts and map tips. Off, each for a reason: GRASS, Oracle and
# SAP HANA have no port; the map server is a FastCGI service; the crash
# handler offers to send a report upstream. nlohmann-json is the system copy;
# poly2tri and meshoptimizer have no port and are the bundled copies. DB
# Manager reaches PostGIS through psycopg2; MetaSearch, the catalogue client,
# runs on OWSLib. Every translation except English is removed after the
# install: bundled data is English only.
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
	-DWITH_INTERNAL_MESHOPTIMIZER=TRUE \
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
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/qgis"
test -x "$PKG/usr/bin/qgis_process"
ls "$PKG"/usr/lib/python3*/site-packages/qgis/_core*.so >/dev/null
find "$PKG/usr/share/qgis/i18n" -name '*.qm' ! -name 'qgis_en_US.qm' -delete

# The project and layer file types QGIS defines are shipped only in upstream's
# distribution packaging, not by its CMake.
install -Dm644 rpm/sources/qgis-mime.xml "$PKG/usr/share/mime/packages/qgis.xml"

# UPSTREAM'S ENTRY IS REPLACED: the Wayland app_id is org.qgis.qgis (the
# organisation domain reversed, then the program's name) where upstream writes
# the X11 class QGIS3; it carries every translation; and it claims TIFF, JPEG
# and the GIS formats every image viewer and GDAL tool also opens. It keeps
# the five QGIS file types the XML above defines.
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
