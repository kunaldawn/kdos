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

# JPEG and PNG output are optional find_package() calls with no switch, so they
# are made required: without them the thumbnailer silently loses a format.
# ENABLE_GIO dlopen()s glib at run time so a file manager can hand it file://
# and trash:// URIs; the build needs only dlfcn.h.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_REQUIRE_FIND_PACKAGE_JPEG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PNG=ON \
	-DENABLE_GIO=ON \
	-DENABLE_THUMBNAILER=ON -DENABLE_AUDIO_THUMBNAILER=ON \
	-DENABLE_TESTS=OFF -DBUILD_SHARED_LIBS=ON
make
make DESTDIR=$PKG install
