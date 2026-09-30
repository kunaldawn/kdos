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

# SpatiaLite: spatial SQL for SQLite, as libspatialite and as mod_spatialite,
# the extension QGIS, GDAL's SQLite driver and Python load at run time. PROJ,
# GEOS (with its advanced and 3.11 functions), RTTOPO, libxml2 (XML
# documents and the ISO metadata functions), MiniZIP (loading zipped
# shapefiles), FreeXL (loading .xls, .xlsx and .ods spreadsheets) and
# GeoPackage are all on. The examples are not built.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-mathsql \
	--enable-proj \
	--enable-iconv \
	--enable-freexl=yes \
	--enable-epsg \
	--enable-geos \
	--enable-gcp \
	--enable-geosadvanced \
	--enable-geosreentrant \
	--enable-rttopo \
	--enable-libxml2 \
	--enable-minizip \
	--enable-geopackage \
	--enable-examples=no
make
make DESTDIR=$PKG install
test -e "$PKG"/usr/lib/mod_spatialite.so
