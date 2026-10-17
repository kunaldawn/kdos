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

patch -p1 -i "$PORT_SRC/no-version-check.patch"

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
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
	-DUSE_SYSTEM_FREETYPE=ON \
	-DUSE_DISCORD=OFF \
	-DUSE_CCACHE=OFF \
	-DHEADLESS=OFF \
	-DUNITTEST=OFF \
	-DATLAS_TOOL=OFF \
	-DGOLD=OFF
ninja
DESTDIR=$PKG ninja install
test ! -e "$PKG/usr/include"
cd ..

find "$PKG/usr/share/ppsspp/assets/lang" -name '*.ini' ! -name en_US.ini -delete

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
