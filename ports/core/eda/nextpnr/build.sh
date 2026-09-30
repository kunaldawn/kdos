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

# THE CHIP DATABASES ARE READ AT BUILD TIME, NOT AT RUN TIME, which is why
# icestorm and prjtrellis are `depends` of a program that does not link them.
# nextpnr compiles each fabric description into its own binary — an ice40
# nextpnr and an ecp5 nextpnr are two executables — so a database installed
# after this port is a database this build never saw.
#
# -DBUILD_GUI=ON is the floorplan viewer, `--gui` on every nextpnr-<arch>
# command: the placed and routed design drawn over the fabric, stepped through
# pack, place and route. It is Qt 6 with OpenGL widgets, and the 3rdparty
# QtPropertyBrowser tree in the tarball is built for it. The GUI requires
# BUILD_PYTHON, which is on below. On any C library but glibc, configure
# stops when libunwind is missing, GUI or not; only the GUI links it, for its
# crash backtraces.
#
# ice40 and ecp5 each get a menu entry running `nextpnr-<arch> --gui` with
# nothing else: the window opens on the family's default part, and its New and
# Open JSON actions pick a device and a synthesised design. machxo2 refuses to
# start without --device, and generic has no device until a Python script
# describes one, so neither has an entry.
#
# -DBUILD_PYTHON=ON embeds Python for --pre-pack, --post-route and --run
# scripts, bound through pybind11. find_package(pybind11 CONFIG) finds nothing
# on the default search path, because the python3-pybind11 wheel keeps its
# CMake package inside site-packages, and a miss falls back to the copy in
# 3rdparty/ without a word. pybind11_DIR names the system copy.
#
# -DUSE_OPENMP=ON parallelises the analytic placer through gcc's libgomp.
#
# machxo2 reads the same prjtrellis database ecp5 does, and compiles a chip
# database per device named in MACHXO2_DEVICES and no other: 1200 is the
# LCMXO2-1200 and 6900 the LCMXO3-6900, upstream's default pair. A design for
# any other MachXO2 or MachXO3 part finds no chipdb and stops. nexus, mistral
# and himbaechel are left out of ARCH: their databases come from prjoxide, the
# mistral source tree and apicula, none of them ports.
mkdir -p build && cd build
# CMP0167=NEW makes find_package(Boost) use BOOSTCONFIG.CMAKE rather than
# CMake's own legacy FindBoost module. The module looks for a `libboost_system`
# FILE on disk, and Boost 1.92 made Boost.System HEADER-ONLY — so it reports
# "Could NOT find Boost (missing: system)" beside a boost that is fully
# installed, right after warning that a new Boost "may have incorrect or
# missing dependencies". The config package upstream ships knows which of its
# own components are header-only.
cmake .. -G Ninja \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
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

# No organisation domain and no desktop file name are set, so Qt makes the
# Wayland app_id the program name, which each entry repeats. nextpnr ships no
# application icon.
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
