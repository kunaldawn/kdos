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

# glog 0.7's headers refuse to compile unless GLOG_USE_GLOG_EXPORT is defined.
# The glog::glog CMake target defines it for a consumer; the installed
# libglog.pc does not, so a consumer that finds glog through pkg-config has to
# define it itself. Stack traces on a fatal message come from libunwind: musl
# has no <execinfo.h> backtrace. WITH_UNWIND=libunwind wants <libunwind.h> and
# libunwind.so on the default paths, which is libunwind-nongnu; LLVM's copy
# sits under /usr/lib/llvm-libunwind where the finder does not look.
# WITH_GFLAGS alone lets a missing gflags pass as a warning and builds glog
# without it; the requirement makes that a failed configure.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DWITH_GFLAGS=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_gflags=ON \
	-DWITH_GTEST=OFF \
	-DWITH_UNWIND=libunwind \
	-DWITH_SYMBOLIZE=ON \
	-DWITH_TLS=ON \
	-DWITH_PKGCONFIG=ON \
	-DPRINT_UNSYMBOLIZED_STACK_TRACES=OFF
ninja
DESTDIR=$PKG ninja install
