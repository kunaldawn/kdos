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

# The native file dialogs are a git submodule, absent from the release
# archive, and without them the build quietly falls back to its own ImGui
# dialog. They are the pinned commit, a second source, and speak to the file
# chooser portal over D-Bus, so no GTK is linked.
rmdir external/nativefiledialog-extended
mv "$SRC_ROOT/nativefiledialog-extended-$_nfd" external/nativefiledialog-extended

# The start-up check for a new release is taken out: on Linux it contacts
# GitHub on every launch unless a Flatpak sandbox is detected. The user's
# own WHDLoad-database download stays, and fails cleanly with no network.
patch -p1 -i "$PORT_SRC/no-update-check.patch"

# SDL 3 directly, desktop OpenGL through libglvnd. Every optional library is
# named, since a missing one is only a status line: libserialport for the
# serial port, ENet and libpcap/TAP for networking, PortMidi for MIDI,
# libmpeg2 for CD32 full-motion video, mpg123 for MP3 CD tracks, zstd for
# CHD images. PCem (the x86 bridgeboards) and the PowerPC boards are on; the
# PowerPC CPU itself is a QEMU-UAE plugin this image does not carry. The GPIO
# LEDs and the D-Bus remote control are off. JIT stays on.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
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

# UPSTREAM'S ENTRY IS REPLACED without application/x-cue: a cue sheet is
# any CD image, and the player that claims it should not be an Amiga. The
# app_id is SDL's default, the executable's name.
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
