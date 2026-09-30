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

# THE iHD DRIVER — the half of Intel video acceleration that libva loads. libva
# is a dispatcher and nothing else: with no driver behind it every VA-API call
# returns VA_STATUS_ERROR_UNIMPLEMENTED and ffmpeg falls back to software. The
# driver installs into ${CMAKE_INSTALL_FULL_LIBDIR}/dri, which is the directory
# libva's own libdir points at, so both must be configured with the same libdir
# or the dispatcher searches a path nothing ever installed into.
#
# MEDIA_BUILD_FATAL_WARNINGS=OFF. The default ON stamps -Werror onto eight
# object libraries of a 30 MB C++ tree, so a warning gcc gained since Intel's
# validated compiler stops the build somewhere unrelated to this machine.
# Warnings still print.
#
# ENABLE_KERNELS=ON compiles in the prebuilt EU shaders this tarball carries.
# OFF is not a size trim: media_feature_flags_linux.cmake then switches off
# every feature that needs one — VME encode for AVC, HEVC and MPEG-2, VC-1
# decode, decode processing, auto denoise, VP8 encode — and the driver reports
# those formats unsupported rather than degrading to a slower path.
#
# BUILD_KERNELS=OFF. Rebuilding the shaders from their assembly needs Intel's
# iga64 and cmc, which this tree does not carry, and cmake answers a missing one
# with FATAL_ERROR rather than falling back to the prebuilt binaries. Upstream
# already forces it off while ENABLE_NONFREE_KERNELS is on; the flag pins it so
# that switching the nonfree shaders off cannot quietly demand those tools.
#
# MEDIA_RUN_TEST_SUITE=OFF. Its subdirectory pulls in googletest, which is not a
# dependency of a driver; the guard around it also tests for a ReleaseInternal
# build, so leaving the option on makes the build type load-bearing.
#
# INSTALL_DRIVER_SYSCONF=OFF. It writes a /etc/profile.d script exporting
# LIBVA_DRIVER_NAME; the driver is chosen by probing the PCI id, and a shell
# profile pinning it is wrong on any machine with a second GPU.
#
# CMAKE_DISABLE_FIND_PACKAGE_X11=ON. The driver has no X11 option of its own:
# it runs find_package(X11), which finds the libX11 that Xwayland brings, and
# then keeps the X path only if libva-x11.pc exists as well. Disabling the find
# keeps the driver off X whatever libX11 and libva happen to carry.
#
# CMAKE_DISABLE_FIND_PACKAGE_Backtrace=ON. musl has no execinfo.h, and the
# driver links whichever backtrace library find_package(Backtrace) turns up,
# so a stray libexecinfo on the builder would become a runtime dependency of
# a driver no recipe declares it for.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DENABLE_KERNELS=ON \
	-DBUILD_KERNELS=OFF \
	-DINSTALL_DRIVER_SYSCONF=OFF \
	-DMEDIA_BUILD_FATAL_WARNINGS=OFF \
	-DMEDIA_RUN_TEST_SUITE=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Backtrace=ON
ninja
DESTDIR=$PKG ninja install
