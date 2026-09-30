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

cmake -GNinja -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_DATADIR=/usr/share \
	-DCMAKE_PREFIX_PATH=/usr \
	-DVULKAN_HEADERS_INSTALL_DIR=/usr \
	-DBUILD_CUBE=ON \
	-DBUILD_VULKANINFO=ON \
	-DBUILD_ICD=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_WSI_WAYLAND_SUPPORT=ON \
	-DBUILD_WSI_DISPLAY_SUPPORT=ON \
	-DBUILD_WSI_XCB_SUPPORT=OFF \
	-DBUILD_WSI_XLIB_SUPPORT=OFF \
	-DBUILD_WSI_DIRECTFB_SUPPORT=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/vkcube.desktop" <<'KDOS_EOF'
[Desktop Entry]
Type=Application
Name=Vulkan Cube
GenericName=Vulkan Test
Comment=Spinning textured cube on the Vulkan driver this machine picked
Exec=vkcube
Icon=video-display
Terminal=false
Categories=Development;Graphics;
Keywords=vulkan;gpu;driver;3d;benchmark;vkcube;
KDOS_EOF
chmod 644 "$PKG/usr/share/applications/vkcube.desktop"
