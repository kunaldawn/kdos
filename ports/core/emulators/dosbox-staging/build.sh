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

# System libraries rather than vcpkg, whose toolchain file downloads every
# dependency. The tests (GTest) and the MkDocs manual are off. MT-32
# emulation is libmt32emu and General MIDI is FluidSynth; both need data the
# player supplies (Roland ROMs, a SoundFont). ManyMouse reads mice through
# evdev, not the X Input protocol. libslirp, for the emulated network card, is
# loaded at run time. The rpath upstream sets points at a lib/ directory beside
# the binary that a system install does not have, so no rpath is written.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_RPATH=ON \
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

# Bundled data is English only; the interface falls back to its built-in
# English messages.
rm -rf "$PKG/usr/share/dosbox-staging/resources/translations"

# UPSTREAM'S ENTRY IS REPLACED for its translations. The Wayland app_id is
# org.dosbox_staging.dosbox_staging, set through SDL's app-id hint; the icon
# of that name is upstream's hicolor PNG set, installed above.
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
