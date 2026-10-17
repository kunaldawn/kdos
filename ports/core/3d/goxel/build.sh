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

patch -p1 -i "$PORT_SRC/goxel-app-id.patch"

scons -j"$KDOS_JOBS" mode=release werror=false nfd_backend=portal sound=false
make PREFIX=/usr DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/goxel.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Goxel
GenericName=Voxel Editor
Comment=Build 3D models out of voxels
TryExec=goxel
Exec=goxel %f
Icon=goxel
Terminal=false
StartupWMClass=goxel
Categories=Graphics;3DGraphics;
Keywords=voxel;3d;model;editor;pixel;goxel;
DESKTOP
chmod 644 "$PKG/usr/share/applications/goxel.desktop"
