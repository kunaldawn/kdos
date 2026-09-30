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

# Adapters: the Pulse-Eight USB adapter, found through udev, and every
# /dev/cec* node of the kernel CEC framework. The physical address is read
# from the DRM EDID in sysfs; the xrandr route is off, as is every SoC
# backend. DISABLE_BUILDINFO keeps the build user, host and date out of the
# version string, so two builds of one tree are identical. The Python module
# is generated with SWIG and is what pyCecClient imports. cec-client links
# curses for its interactive prompt.
mkdir build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
	-DDISABLE_STATIC=ON -DDISABLE_BUILDINFO=ON -DDISABLE_CLIENT=OFF \
	-DHAVE_LIBUDEV=ON -DHAVE_LINUX_API=ON -DHAVE_DRM_EDID_PARSER=ON \
	-DHAVE_RANDR=OFF -DHAVE_RPI_API=OFF -DHAVE_TDA995X_API=OFF \
	-DHAVE_EXYNOS_API=OFF -DHAVE_AOCEC_API=OFF -DHAVE_IMX_API=OFF \
	-DHAVE_TEGRA_API=OFF \
	-DSKIP_PYTHON_WRAPPER=0 -DPYTHON_PKG_DIR=site-packages \
	-DENABLE_DOTNET_LIB=OFF -DENABLE_NODE_LIB=OFF -DENABLE_RUST_LIB=OFF
make
make DESTDIR=$PKG install
