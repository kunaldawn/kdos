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

# The new-version check is taken out: on Linux it fetches ppsspp.org's
# version file every fifth start unless the user finds the setting first.
patch -p1 -i "$PORT_SRC/no-version-check.patch"

# THE SYSTEM FFmpeg, NEVER THE BUNDLED ONE. The release archive carries
# ffmpeg/linux/x86_64/lib, prebuilt static libraries that nothing here
# compiled; USE_SYSTEM_FFMPEG keeps the link off them.
#
# The SDL front end on sdl2-compat. Vulkan presents through its Wayland WSI;
# the X11 WSI is off, because sdl2-compat has no X11 window to hand it.
# OpenGL is desktop GL through libglvnd. Discord presence is off. UPnP for
# ad hoc play uses the system miniupnpc, and only when the user turns it on.
# snappy and libchdr are bundled copies, built from source, because neither
# is a port.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DUSING_QT_UI=OFF \
	-DUSING_GLES2=OFF \
	-DUSING_EGL=OFF \
	-DUSING_FBDEV=OFF \
	-DUSING_X11_VULKAN=OFF \
	-DUSE_WAYLAND_WSI=ON \
	-DUSE_VULKAN_DISPLAY_KHR=OFF \
	-DUSE_FFMPEG=ON \
	-DUSE_SYSTEM_FFMPEG=ON \
	-DUSE_SYSTEM_LIBSDL2=ON \
	-DUSE_SYSTEM_LIBPNG=ON \
	-DUSE_SYSTEM_LIBZIP=ON \
	-DUSE_SYSTEM_ZSTD=ON \
	-DUSE_SYSTEM_RAPIDJSON=ON \
	-DUSE_MINIUPNPC=ON \
	-DUSE_SYSTEM_MINIUPNPC=ON \
	-DUSE_SYSTEM_SNAPPY=OFF \
	-DUSE_SYSTEM_LIBCHDR=OFF \
	-DUSE_DISCORD=OFF \
	-DUSE_CCACHE=OFF \
	-DHEADLESS=OFF \
	-DUNITTEST=OFF \
	-DATLAS_TOOL=OFF \
	-DGOLD=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

# English only: the interface falls back to en_US for every string.
find "$PKG/usr/share/ppsspp/assets/lang" -name '*.ini' ! -name en_US.ini -delete

# UPSTREAM'S ENTRY IS REPLACED. Its StartupWMClass names an id nothing in the
# SDL front end sets; SDL's Wayland app_id is the executable's name. Its
# MimeType claims every CD image and every ZIP file; this one claims only
# the compressed ISO that ppsspp.xml defines.
cat > "$PKG/usr/share/applications/PPSSPPSDL.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=PPSSPP
GenericName=PSP Emulator
Comment=Emulate the Sony PlayStation Portable
TryExec=PPSSPPSDL
Exec=PPSSPPSDL %f
Icon=ppsspp
Terminal=false
StartupWMClass=PPSSPPSDL
MimeType=application/x-compressed-iso;
Categories=Game;Emulator;
Keywords=sony;playstation;portable;psp;handheld;emulator;
DESKTOP
chmod 644 "$PKG/usr/share/applications/PPSSPPSDL.desktop"
