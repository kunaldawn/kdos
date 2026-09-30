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


# The release archive carries the qucsator-rf, RxCalc and S-parameter viewer
# submodules; the git archive of the tag does not, and without them the RF
# simulator and two tools are silently left out. ngspice is the default
# simulator and is found on PATH at run time. Git is not searched for: the
# tarball is no checkout, and the version string then reads the same on every
# builder.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON \
	-DWITH_ADMS=OFF \
	-DUPDATE_TRANSLATIONS=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

# Bundled data is English only; Qt falls back to the source strings.
rm -rf "$PKG/usr/share/qucs-s/lang"

# The upstream entry has no StartupWMClass; the viewer's is not installed.
# Qt takes the Wayland app_id and the X11 class from the executable's name,
# and the viewer's executable is qucs-sspar-viewer.
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
