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

patch -p1 -i "$PORT_SRC/gperfapphash-no-dos2unix.patch"

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DWITH_ADMS=OFF \
	-DUPDATE_TRANSLATIONS=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

rm -rf "$PKG/usr/share/qucs-s/lang"

install -Dm644 qucs-s-spar-viewer/qucs-s-spar-viewer.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/qucs-s-spar-viewer.png"
cat > "$PKG/usr/share/applications/qucs-s.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Qucs-S
GenericName=Circuit Simulator
Comment=Draw a schematic and simulate it with ngspice or qucsator
Exec=qucs-s %F
Icon=qucs-s
Terminal=false
Categories=Qt;Education;Science;Electronics;Engineering;
Keywords=circuit;schematic;spice;ngspice;simulation;electronics;filter;rf;
StartupWMClass=qucs-s
DESKTOP
cat > "$PKG/usr/share/applications/qucs-s-spar-viewer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Qucs-S S-parameter Viewer
GenericName=S-parameter Viewer
Comment=View Touchstone S-parameter files and synthesise RF matching networks
Exec=qucs-sspar-viewer %F
Icon=qucs-s-spar-viewer
Terminal=false
Categories=Qt;Science;Electronics;Engineering;
Keywords=s-parameter;touchstone;smith;rf;matching;
StartupWMClass=qucs-sspar-viewer
DESKTOP
chmod 644 "$PKG"/usr/share/applications/qucs-s*.desktop

test -x "$PKG/usr/bin/qucs-s"
test -x "$PKG/usr/bin/qucsator_rf"
test -x "$PKG/usr/bin/qucs-sspar-viewer"
test -f "$PKG/usr/share/icons/hicolor/256x256/apps/qucs-s.png"
