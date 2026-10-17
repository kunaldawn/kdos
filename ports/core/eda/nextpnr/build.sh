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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCURRENT_GIT_VERSION="nextpnr-$version" \
	-DARCH="ice40;ecp5;machxo2;generic" \
	-DMACHXO2_DEVICES="1200;6900" \
	-DBUILD_GUI=ON \
	-DBUILD_PYTHON=ON \
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" \
	-DBUILD_RUST=OFF \
	-DBUILD_TESTS=OFF \
	-DUSE_OPENMP=ON \
	-DICESTORM_INSTALL_PREFIX=/usr \
	-DTRELLIS_INSTALL_PREFIX=/usr
ninja
DESTDIR=$PKG ninja install

install -d "$PKG/usr/share/applications"
for arch in ice40:iCE40 ecp5:ECP5; do
	cat > "$PKG/usr/share/applications/nextpnr-${arch%%:*}.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=nextpnr (${arch#*:})
GenericName=FPGA Place and Route
Comment=Place and route a design on a Lattice ${arch#*:} FPGA and view the floorplan
Exec=nextpnr-${arch%%:*} --gui
TryExec=nextpnr-${arch%%:*}
Icon=cpu
Terminal=false
StartupWMClass=nextpnr-${arch%%:*}
Categories=Qt;Development;Electronics;
Keywords=fpga;place;route;floorplan;lattice;${arch%%:*};
DESKTOP
	chmod 644 "$PKG/usr/share/applications/nextpnr-${arch%%:*}.desktop"
done
