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

rmdir cpu_features
ln -s "$SRC_ROOT/cpu_features-$_cpufeatures" cpu_features

export CXXFLAGS="$CXXFLAGS -DFMT_DEPRECATED_HEAVY_CORE"
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DVOLK_CPU_FEATURES=ON -DENABLE_ORC=ON -DENABLE_TESTING=OFF \
	-DENABLE_PROFILING=OFF -DENABLE_MODTOOL=ON -DENABLE_STATIC_LIBS=OFF
make
make DESTDIR=$PKG install
rm -f "$PKG"/usr/lib/python3*/site-packages/volk_modtool/*.py[co]
