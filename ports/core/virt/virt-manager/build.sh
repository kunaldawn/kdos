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

# New guests get a VNC display: qemu here is built without SPICE, and a
# default of spice would make every new machine fail to start. spice-gtk is
# still a dependency, for guests on another host that offer SPICE. Only the
# QEMU hypervisor is offered: libvirt here is built with its qemu driver and
# no other. The schema and icon caches are shared indexes that kpkg rebuilds
# on install.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dcompile-schemas=false \
	-Dupdate-icon-cache=false \
	-Ddefault-graphics=vnc \
	-Ddefault-hvs=qemu \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass. GTK 3 takes a toplevel's
# Wayland app_id from the program name, not from the GtkApplication id, and
# virt-manager's windows are never added to its application, so the app_id
# is virt-manager.
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
