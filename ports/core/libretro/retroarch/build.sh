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

patch -p1 -i "$PORT_SRC/config.patch"
patch -p1 -i "$PORT_SRC/database-path.patch"

export CFLAGS="${CFLAGS/-std=gnu11/}"
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--enable-wayland \
	--enable-libdecor \
	--enable-egl \
	--enable-opengl \
	--enable-opengl_core \
	--enable-vulkan \
	--enable-kms \
	--disable-x11 \
	--disable-qt \
	--enable-udev \
	--enable-hid \
	--enable-libusb \
	--enable-sdl2 \
	--disable-sdl \
	--enable-alsa \
	--enable-pulse \
	--enable-pipewire \
	--disable-jack \
	--disable-oss \
	--disable-rsound \
	--disable-roar \
	--disable-tinyalsa \
	--disable-audioio \
	--disable-systemd \
	--enable-freetype \
	--disable-ffmpeg \
	--disable-mpv \
	--enable-networking \
	--disable-ssl \
	--disable-online_updater \
	--disable-update_cores \
	--disable-update_core_info \
	--disable-update_assets \
	--disable-cheevos \
	--disable-discord \
	--disable-translate \
	--disable-langextra \
	--disable-builtinzlib \
	--enable-builtinflac \
	--disable-cg \
	--disable-caca \
	--disable-sixel \
	--disable-vg \
	--disable-videocore
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/bin/retroarch-cg2glsl" "$PKG/usr/share/man/man6/retroarch-cg2glsl.6"

install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 media/com.libretro.RetroArch.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/com.libretro.RetroArch.png"

cat > "$PKG/usr/share/applications/com.libretro.RetroArch.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RetroArch
GenericName=Game Emulator
Comment=Play console and computer games through libretro emulator cores
TryExec=retroarch
Exec=retroarch
Icon=com.libretro.RetroArch
Terminal=false
StartupNotify=true
StartupWMClass=com.libretro.RetroArch
SingleMainWindow=true
PrefersNonDefaultGPU=true
Categories=Game;Emulator;
Keywords=retro;gaming;emulator;console;libretro;nes;snes;genesis;playstation;n64;gameboy;
DESKTOP
chmod 644 "$PKG/usr/share/applications/com.libretro.RetroArch.desktop"
