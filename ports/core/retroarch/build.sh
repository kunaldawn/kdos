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

# The skeleton /etc/retroarch.cfg a first run copies names the system
# directories the data and core ports install into: cores in
# /usr/lib/libretro, and assets, core info, databases, cheats and controller
# profiles under /usr/share/libretro. Without it every one of them resolves
# under the player's own configuration directory, which is empty.
patch -p1 -i "$PORT_SRC/config.patch"
patch -p1 -i "$PORT_SRC/database-path.patch"

# Wayland with EGL, desktop GL, GL core and Vulkan; KMS for running on a bare
# console. X11 is off: the Wayland driver draws the window, and Xwayland is not
# needed for it. Audio goes to PipeWire, Pulse or ALSA, never JACK, OSS or the
# network sound servers. Every online feature is off: the core, asset and info
# updaters (cores and data come from ports), RetroAchievements, Discord and
# the translation service. Networking stays for netplay on a local network,
# which needs no TLS. FFmpeg is off: its recording path and media core do not
# build against the FFmpeg this tree carries. The Qt companion window is off.
# Extra languages are off: bundled data is English only. FLAC is the bundled
# copy, which the CHD reader is written against.
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

# cg2glsl converts Nvidia Cg shaders, which this build cannot load, and is a
# Python script nothing here runs.
rm -f "$PKG/usr/bin/retroarch-cg2glsl" "$PKG/usr/share/man/man6/retroarch-cg2glsl.6"

# The panel reads no SVG: the menu icon is a PNG rasterised from upstream's.
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 media/com.libretro.RetroArch.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/com.libretro.RetroArch.png"

# UPSTREAM'S ENTRY IS REPLACED: it carries forty translations and claims
# every ROM type, including the Game Boy Advance types mgba's entry claims,
# which would open those files in whichever entry sorted first. RetroArch
# opens games from its own menu. The Wayland app_id is com.libretro.RetroArch.
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
