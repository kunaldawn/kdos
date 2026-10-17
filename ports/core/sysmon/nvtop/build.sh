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
