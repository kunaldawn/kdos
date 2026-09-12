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
	-DTPU_SUPPORT=OFF
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
Icon=chart-line
Terminal=true
Categories=System;Monitor;
Keywords=gpu;nvidia;amd;intel;nvtop;
EOF
chmod 644 "$PKG/usr/share/applications/nvtop.desktop"
