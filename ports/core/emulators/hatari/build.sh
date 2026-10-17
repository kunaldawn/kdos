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

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_CapsImage=ON \
	-DENABLE_MAN_PAGES=ON \
	-DENABLE_WERROR=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

test -x "$PKG/usr/bin/hatariui"

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
