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

# The release archive carries the cpu_features submodule as an empty
# directory; the pinned release is the second source. volk builds it as a
# static, uninstalled part of libvolk.
rmdir cpu_features
ln -s "$SRC_ROOT/cpu_features-$_cpufeatures" cpu_features

# fmt must be found as a package: without it the configure step clones fmt
# with FetchContent, and FETCHCONTENT_FULLY_DISCONNECTED turns that into a
# failure here rather than a network fetch.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DVOLK_CPU_FEATURES=ON -DENABLE_ORC=ON -DENABLE_TESTING=OFF \
	-DENABLE_PROFILING=OFF -DENABLE_MODTOOL=ON -DENABLE_STATIC_LIBS=OFF
make
make DESTDIR=$PKG install
