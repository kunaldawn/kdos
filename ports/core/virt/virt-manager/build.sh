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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dcompile-schemas=false \
	-Dupdate-icon-cache=false \
	-Ddefault-graphics=vnc \
	-Ddefault-hvs=qemu \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

cat > "$PKG/usr/share/applications/virt-manager.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Virtual Machine Manager
GenericName=Virtual Machines
Comment=Create, run and connect to virtual machines
Exec=virt-manager
Icon=virt-manager
Terminal=false
StartupWMClass=virt-manager
Categories=System;Emulator;GTK;
Keywords=virtualization;libvirt;vm;vmm;qemu;kvm;virt-manager;
EOF2
chmod 644 "$PKG/usr/share/applications/virt-manager.desktop"
