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
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_PYTHON_BINDINGS=ON \
	-DBUILD_JAVA_BINDINGS=OFF \
	-DBUILD_CSHARP_BINDINGS=OFF \
	-DBUILD_TESTING=OFF \
	-DGDAL_USE_EXTERNAL_LIBS=OFF \
	-DGDAL_USE_CURL=OFF \
	-DGDAL_USE_POPPLER=ON \
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
	-DGDAL_USE_QHULL=ON \
	-DGDAL_USE_FREEXL=ON \
	-DGDAL_USE_XERCESC=ON \
	-DGDAL_USE_SPATIALITE=ON \
	-DGDAL_USE_BLOSC=ON
ninja
DESTDIR=$PKG ninja install
ls "$PKG"/usr/lib/python3*/site-packages/osgeo/_gdal_array*.so >/dev/null 2>&1 || {
	echo 'gdal: osgeo.gdal_array was not built; numpy was not found' >&2
	exit 1
}
