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

rmdir external/nativefiledialog-extended
mv "$SRC_ROOT/nativefiledialog-extended-$_nfd" external/nativefiledialog-extended
rmdir external/nativefiledialog-extended/3ps/wayland-protocols
ln -s /usr/share/wayland-protocols external/nativefiledialog-extended/3ps/wayland-protocols

patch -p1 -i "$PORT_SRC/no-update-check.patch"

patch -p1 -i "$PORT_SRC/musl-null.patch"
patch -p1 -i "$PORT_SRC/system-tinyxml2.patch"
rm src/tinyxml2.cpp src/include/tinyxml2.h
export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUSE_OPENGL=ON \
	-DUSE_GLES=OFF \
	-DUSE_VULKAN=OFF \
	-DUSE_JIT=ON \
	-DUSE_PCEM=ON \
	-DUSE_PPC=ON \
	-DUSE_QEMU_PPC=ON \
	-DUSE_LIBSERIALPORT=ON \
	-DUSE_LIBENET=ON \
	-DUSE_PORTMIDI=ON \
	-DUSE_LIBMPEG2=ON \
	-DUSE_MPG123=ON \
	-DUSE_UAENET_PCAP=ON \
	-DUSE_UAENET_TAP=ON \
	-DUSE_ZSTD=ON \
	-DUSE_GPIOD=OFF \
	-DUSE_DBUS=OFF \
	-DUSE_IPC_SOCKET=ON \
	-DWITH_LTO=OFF \
	-DWITH_OPTIMIZE=OFF \
	-DBUNDLE_SDL=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

cat > "$PKG/usr/share/applications/Amiberry.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Amiberry
GenericName=Amiga Emulator
Comment=Emulate the Commodore Amiga, from the A500 to the A4000 and CD32
TryExec=amiberry
Exec=amiberry %f
Icon=amiberry
Terminal=false
StartupWMClass=amiberry
MimeType=application/x-SPS-virtual-media-image;application/x-DMS-compressed-disk-image;application/x-amiga-disk-image;application/x-WHDLoad-game-archive;application/x-UAE-savestate-file;application/vnd.cloanto.rp9;
Categories=Game;Emulator;
Keywords=amiga;emulator;retro;commodore;a500;a1200;a4000;cd32;whdload;uae;
DESKTOP
chmod 644 "$PKG/usr/share/applications/Amiberry.desktop"
