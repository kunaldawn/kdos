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

# GLFW sets no Wayland app_id unless asked, so the window could never be
# matched to its menu entry; the patch names it goxel.
patch -p1 -i "$PORT_SRC/goxel-app-id.patch"

# nfd_backend=portal: file dialogs go over D-Bus to the FileChooser portal
# instead of through GTK. mode=release, because the default is a debug build
# linked against the address and undefined-behaviour sanitisers. werror=false
# keeps a newer compiler's warnings from failing the build. Sound stays off:
# it is a few interface clicks and needs OpenAL.
scons -j"$KDOS_JOBS" mode=release werror=false nfd_backend=portal sound=false
make PREFIX=/usr DESTDIR=$PKG install

# UPSTREAM'S ENTRY IS REPLACED, with the app_id the patch sets.
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
