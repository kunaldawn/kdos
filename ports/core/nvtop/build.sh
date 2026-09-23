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

# -DNVIDIA_SUPPORT=OFF, AND THAT IS NOT A LIMITATION HERE. The NVIDIA backend
# links NVML out of the proprietary driver, which this distro does not ship and
# cannot build; the AMD, Intel and generic backends all read `drm-engine-*` out
# of /proc/<pid>/fdinfo — the SAME source kdos-energyd's GPU column already
# uses, so the two agree about what a GPU was doing by construction.
#
# A driver with no fdinfo stats gets no reading rather than a zero, which is
# kdos-res's rule stated by somebody else's program.
#
# METAX, ENFLAME, V3D and ROCKCHIP default on and are pinned off with the
# other ARM backends: the first two link proprietary vendor libraries this
# distro cannot ship, the last two drive Raspberry Pi and Rockchip boards.
# USE_LIBUDEV_OVER_LIBSYSTEMD names eudev's libudev as the device-discovery
# library rather than leaving it to whichever of the two is found.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DNVIDIA_SUPPORT=OFF \
	-DAMDGPU_SUPPORT=ON \
	-DINTEL_SUPPORT=ON \
	-DMSM_SUPPORT=OFF \
	-DPANFROST_SUPPORT=OFF \
	-DPANTHOR_SUPPORT=OFF \
	-DASCEND_SUPPORT=OFF \
	-DTPU_SUPPORT=OFF \
	-DV3D_SUPPORT=OFF \
	-DROCKCHIP_SUPPORT=OFF \
	-DMETAX_SUPPORT=OFF \
	-DENFLAME_SUPPORT=OFF \
	-DUSE_LIBUDEV_OVER_LIBSYSTEMD=ON
ninja
DESTDIR=$PKG ninja install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/nvtop.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=GPU Monitor
GenericName=GPU Monitor
Comment=GPU load, memory and processes
Exec=nvtop
Icon=video-display
Terminal=true
Categories=System;Monitor;
Keywords=gpu;nvidia;amd;intel;nvtop;
EOF
chmod 644 "$PKG/usr/share/applications/nvtop.desktop"
