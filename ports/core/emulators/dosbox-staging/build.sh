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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_SKIP_RPATH=ON \
	-DHAVE_PTHREAD_SETNAME_NP=1 \
	-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON \
	-DIS_PRESET_USED=TRUE \
	-DUSE_SYSTEM_LIBS=ON \
	-DOPT_TESTS=OFF \
	-DOPT_DOCUMENTATION=OFF \
	-DOPT_DEBUGGER=OFF \
	-DOPT_OPENGL=ON \
	-DOPT_MT32EMU=ON \
	-DOPT_MANYMOUSE=ON \
	-DOPT_XINPUT=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

rm -rf "$PKG/usr/share/dosbox-staging/resources/translations"

cat > "$PKG/usr/share/applications/org.dosbox_staging.dosbox_staging.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DOSBox Staging
GenericName=DOS Emulator
Comment=Run DOS games and programs on an emulated PC
TryExec=dosbox
Exec=dosbox
Icon=org.dosbox_staging.dosbox_staging
Terminal=false
StartupWMClass=org.dosbox_staging.dosbox_staging
Categories=Game;Emulator;
Keywords=dos;msdos;freedos;pc;game;emulator;dosbox;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.dosbox_staging.dosbox_staging.desktop"
