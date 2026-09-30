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

# Every optional library is present, because each is found or quietly left
# out: zlib for zipped disk images, libpng for screenshots and AVI frames,
# readline for the debugger's history, PortMidi for MIDI out, Capstone (with
# its m68k module) for the disassembler, udev for joystick hot-plugging.
# IPF images through the closed CAPS library are not supported.
#
# X11 IS NOT LOOKED FOR. Hatari reads the X11 window of its SDL surface for
# embedding, and sdl2-compat is built without X11 hooks, so there is none to
# read.
#
# THE HATARI UI IS BUILT because the GTK 3 bindings are present at configure
# time, which python3-gobject and gtk3 in depends guarantee; without them the
# front end is dropped with no message.
#
# Hatari carries no TOS. The user supplies a TOS or EmuTOS image, which the
# first-start dialog asks for.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_CapsImage=ON \
	-DENABLE_MAN_PAGES=ON \
	-DENABLE_WERROR=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

# The GTK probe fails silently, and a missing front end would leave its menu
# entry below naming a program that is not there.
test -x "$PKG/usr/bin/hatariui"

# BOTH ENTRIES ARE REPLACED for StartupWMClass. Hatari sets its SDL window
# class to hatari, which is the Wayland app_id. The Hatari UI is PyGObject
# with no application id, so GTK takes the program name, the script's
# basename.
cat > "$PKG/usr/share/applications/hatari.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Hatari
GenericName=Atari ST Emulator
Comment=Emulate the Atari ST, STE, TT and Falcon computers
TryExec=hatari
Exec=hatari %f
Icon=hatari
Terminal=false
StartupWMClass=hatari
MimeType=application/x-st-disk-image;application/vnd.msa-disk-image;application/vnd.fastcopy-disk-image;application/x-stx-disk-image;
Categories=Game;Emulator;
Keywords=emulator;atari;atari st;ste;falcon;tos;retro;
DESKTOP
cat > "$PKG/usr/share/applications/hatariui.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Hatari UI
GenericName=Atari ST Emulator Front End
Comment=Configure and control the Hatari Atari ST emulator
TryExec=hatariui
Exec=hatariui
Icon=hatari
Terminal=false
StartupWMClass=hatariui.py
Categories=Game;Emulator;
Keywords=emulator;atari;atari st;hatari;settings;
DESKTOP
chmod 644 "$PKG/usr/share/applications/hatari.desktop" \
	"$PKG/usr/share/applications/hatariui.desktop"
