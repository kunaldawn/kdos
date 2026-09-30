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

# The release archive carries third_party/ with every submodule empty; each
# dependency is taken from the system, and the CMAKE_REQUIRE_FIND_PACKAGE_*
# switches turn a codec cjxl/djxl would otherwise drop silently into a
# configure failure. image/jxl is defined by shared-mime-info, so the MIME
# plugin stays off.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DJPEGXL_FORCE_SYSTEM_HWY=ON \
	-DJPEGXL_FORCE_SYSTEM_BROTLI=ON \
	-DJPEGXL_FORCE_SYSTEM_LCMS2=ON \
	-DJPEGXL_BUNDLE_LIBPNG=OFF \
	-DJPEGXL_ENABLE_SKCMS=OFF \
	-DJPEGXL_ENABLE_SJPEG=OFF \
	-DJPEGXL_ENABLE_OPENEXR=ON \
	-DJPEGXL_ENABLE_TRANSCODE_JPEG=ON \
	-DJPEGXL_ENABLE_BOXES=ON \
	-DJPEGXL_ENABLE_TOOLS=ON \
	-DJPEGXL_ENABLE_MANPAGES=ON \
	-DJPEGXL_ENABLE_DOXYGEN=OFF \
	-DJPEGXL_ENABLE_BENCHMARK=OFF \
	-DJPEGXL_ENABLE_EXAMPLES=OFF \
	-DJPEGXL_ENABLE_DEVTOOLS=OFF \
	-DJPEGXL_ENABLE_VIEWERS=OFF \
	-DJPEGXL_ENABLE_FUZZERS=OFF \
	-DJPEGXL_ENABLE_JNI=OFF \
	-DJPEGXL_ENABLE_TCMALLOC=OFF \
	-DJPEGXL_ENABLE_COVERAGE=OFF \
	-DJPEGXL_ENABLE_PLUGINS=ON \
	-DJPEGXL_ENABLE_PLUGIN_GDKPIXBUF=ON \
	-DJPEGXL_ENABLE_PLUGIN_MIME=OFF \
	-DJPEGXL_WARNINGS_AS_ERRORS=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_GIF=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_JPEG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PNG=ON
cmake --build build
DESTDIR=$PKG cmake --install build
