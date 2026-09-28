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

# CMake caches the first expat it finds under two names and then refuses the
# second lookup; the patch makes both lookups uncached.
patch -p1 -i "$PORT_SRC/fix-findexpat.patch"

# loguru prints a stack trace through execinfo.h, which musl does not have.
export CXXFLAGS="$CXXFLAGS -DLOGURU_STACKTRACES=0"

# Render windows: X11 and EGL are both built and chosen at run time, so a
# consumer gets an X11 window under Xwayland and an EGL context offscreen.
# External libraries are this tree's ports wherever one exists. hdf5 stays
# bundled and symbol-mangled (vtkhdf5_*): VTK 9.5 is written against the 1.x
# API, the hdf5 port is 2.x, and the mangled copy cannot clash with it. The
# other bundled ones have no port here, or are VTK's own fork (libharu).
# Python wrapping is on: FreeCAD's FEM post-processing filters are VTK Python
# objects, and without it FreeCAD builds FEM without them.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DVTK_BUILD_TESTING=OFF \
	-DVTK_BUILD_EXAMPLES=OFF \
	-DVTK_BUILD_DOCUMENTATION=OFF \
	-DVTK_USE_LARGE_DATA=OFF \
	-DVTK_USE_MPI=OFF \
	-DVTK_USE_CUDA=OFF \
	-DVTK_USE_X=ON \
	-DVTK_OPENGL_HAS_EGL=ON \
	-DVTK_OPENGL_USE_GLES=OFF \
	-DVTK_WRAP_PYTHON=ON \
	-DVTK_WRAP_JAVA=OFF \
	-DPython3_EXECUTABLE=/usr/bin/python3 \
	-DVTK_GROUP_ENABLE_Qt=NO \
	-DVTK_GROUP_ENABLE_MPI=NO \
	-DVTK_MODULE_ENABLE_VTK_IOFFMPEG=NO \
	-DVTK_MODULE_ENABLE_VTK_IOOCCT=NO \
	-DVTK_MODULE_USE_EXTERNAL_VTK_doubleconversion=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_eigen=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_expat=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_fmt=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_freetype=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_jpeg=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_jsoncpp=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_libproj=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_libxml2=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_lz4=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_lzma=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_netcdf=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_nlohmannjson=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_ogg=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_png=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_pugixml=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_sqlite=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_theora=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_tiff=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_zlib=ON \
	-DVTK_MODULE_USE_EXTERNAL_VTK_hdf5=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_libharu=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_cgns=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_cli11=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_exprtk=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_fast_float=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_gl2ps=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_ioss=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_pegtl=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_scn=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_token=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_utf8=OFF \
	-DVTK_MODULE_USE_EXTERNAL_VTK_verdict=OFF
ninja
DESTDIR=$PKG ninja install
rm -rf "$PKG/usr/share/doc"
