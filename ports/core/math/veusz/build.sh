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

export QMAKE_EXE=/usr/lib/qt6/bin/qmake
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/veusz/helpers" -maxdepth 1 -name 'qtloops*.so')"
test -n "$(find "$site/veusz/helpers" -maxdepth 1 -name 'threed*.so')"

install -Dm644 Documents/man-page/veusz.1 -t "$PKG/usr/share/man/man1"
install -Dm644 support/veusz.xml "$PKG/usr/share/mime/packages/veusz.xml"
for s in 16 32 48 64 128; do
	install -Dm644 icons/veusz_$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/veusz.png"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/veusz.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Veusz
GenericName=Scientific Plotting
Comment=Publication-quality 2D and 3D scientific plots
TryExec=veusz
Exec=veusz %F
Icon=veusz
Terminal=false
StartupWMClass=veusz
MimeType=application/x-veusz;
Categories=Qt;Education;Science;DataVisualization;
Keywords=graphing;plotting;graph;plot;chart;visualisation;science;data;veusz;
DESKTOP
chmod 644 "$PKG/usr/share/applications/veusz.desktop"
