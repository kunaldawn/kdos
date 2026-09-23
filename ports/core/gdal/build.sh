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

mkdir -p build && cd build
# GDAL_USE_EXTERNAL_LIBS=OFF turns every optional library off unless it is
# named here, and a library named ON that is missing fails the configure. The
# drivers therefore follow this list and the depends line, never whatever the
# build root happened to hold. What is not named falls back to GDAL's internal
# copy where one exists (geotiff, lerc, shapelib) and is otherwise absent.
#
# GDAL_USE_CURL=OFF for PROJ's reason: /vsicurl/ turns a dataset path into an
# HTTP client, and a format library that reaches the network is a format
# library that fails differently depending on who is listening. Everything here
# reads from disk.
#
# GDAL_USE_POPPLER=OFF: the PDF driver compiles against poppler's private
# headers, which the poppler port does not install
# (ENABLE_UNSTABLE_API_ABI_HEADERS), so turning it on fails the build.
# GDAL_USE_PDFIUM=OFF: GDAL wants its own patched static PDFium, not the
# pdfium port's.
#
# No python bindings: nothing on the image imports osgeo.
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_PYTHON_BINDINGS=OFF \
	-DBUILD_JAVA_BINDINGS=OFF \
	-DBUILD_CSHARP_BINDINGS=OFF \
	-DBUILD_TESTING=OFF \
	-DGDAL_USE_EXTERNAL_LIBS=OFF \
	-DGDAL_USE_CURL=OFF \
	-DGDAL_USE_POPPLER=OFF \
	-DGDAL_USE_PDFIUM=OFF \
	-DGDAL_USE_ZLIB=ON \
	-DGDAL_USE_ICONV=ON \
	-DGDAL_USE_OPENMP=ON \
	-DGDAL_USE_GEOS=ON \
	-DGDAL_USE_SQLITE3=ON \
	-DGDAL_USE_PCRE2=ON \
	-DGDAL_USE_POSTGRESQL=ON \
	-DGDAL_USE_TIFF=ON \
	-DGDAL_USE_JPEG=ON \
	-DGDAL_USE_PNG=ON \
	-DGDAL_USE_GIF=ON \
	-DGDAL_USE_WEBP=ON \
	-DGDAL_USE_OPENJPEG=ON \
	-DGDAL_USE_HEIF=ON \
	-DGDAL_USE_AVIF=ON \
	-DGDAL_USE_JXL=ON \
	-DGDAL_USE_JXL_THREADS=ON \
	-DGDAL_USE_OPENEXR=ON \
	-DGDAL_USE_CFITSIO=ON \
	-DGDAL_USE_NETCDF=ON \
	-DGDAL_USE_HDF5=ON \
	-DGDAL_USE_LIBAEC=ON \
	-DGDAL_USE_ZSTD=ON \
	-DGDAL_USE_DEFLATE=ON \
	-DGDAL_USE_LZ4=ON \
	-DGDAL_USE_LIBLZMA=ON \
	-DGDAL_USE_ARCHIVE=ON \
	-DGDAL_USE_EXPAT=ON \
	-DGDAL_USE_LIBXML2=ON \
	-DGDAL_USE_OPENSSL=ON \
	-DGDAL_USE_JSONC=ON \
	-DGDAL_USE_QHULL=ON
ninja
DESTDIR=$PKG ninja install
