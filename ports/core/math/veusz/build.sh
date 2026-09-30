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

# Veusz's compiled helpers (the qtloops painting routines and the threed
# scene renderer) are SIP extensions built against PyQt6's .sip files with
# the system qmake. The examples install with the resources and open from the
# Help menu. The optional features are imported at run time, so each is a
# dependency and not a build switch: FITS import (astropy), HDF5 import
# (h5py), EMF export (pyemf3), Minuit fitting (iminuit) and D-Bus control
# (dbus-python).
export QMAKE_EXE=/usr/lib/qt6/bin/qmake
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/veusz/helpers" -maxdepth 1 -name 'qtloops*.so')"
test -n "$(find "$site/veusz/helpers" -maxdepth 1 -name 'threed*.so')"

install -Dm644 Documents/man-page/veusz.1 -t "$PKG/usr/share/man/man1"
install -Dm644 support/veusz.xml "$PKG/usr/share/mime/packages/veusz.xml"
for s in 16 32 48 64 128; do
	install -Dm644 icons/veusz_$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/veusz.png"
done

# Upstream's entry is written by its packagers, not by setup.py. The program
# sets the desktop file name, and so the Wayland app_id, to veusz.
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
