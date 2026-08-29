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

# CONTAINERS SHARE THE HOST KERNEL, SO NOTHING ON THIS MACHINE COULD RUN
# ANOTHER OS. That is the gap: the appbox answers "run somebody else's
# userland" and answers nothing about running somebody else's KERNEL — testing
# a kernel change, opening a disk image whose contents you do not trust,
# booting the installer of the distro you are migrating from. On a distro whose
# own test rig is qemu, the shipped system could not run it.
#
# THREE TARGETS, NOT ALL OF THEM. `--target-list` is the single biggest lever
# on the size of this port: the full set is forty-odd emulators at roughly
# 350-400 MB, and the three here cover running this machine's own architecture
# (x86_64), the boards this tree already cross-compiles for (aarch64), and the
# ISA its FPGA and embedded ring targets (riscv64).
#
# EVERY GRAPHICAL FRONT END IS OFF and there is no loss: --enable-curses gives
# a text console on the cell grid, and --enable-vnc means a guest with a real
# framebuffer is watched from any viewer — which is exactly how this project's
# own testing/vnc-shot.py already drives a VM. GTK and SDL are the hard rule;
# --disable-opengl follows from mesa being built without GLX.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libexecdir=/usr/lib/qemu \
	--target-list=x86_64-softmmu,aarch64-softmmu,riscv64-softmmu \
	--enable-kvm \
	--enable-curses \
	--enable-vnc \
	--enable-slirp \
	--enable-linux-aio \
	--enable-virtfs \
	--enable-tools \
	--disable-gtk \
	--disable-sdl \
	--disable-opengl \
	--disable-virglrenderer \
	--disable-spice \
	--disable-vte \
	--disable-xen \
	--disable-docs \
	--disable-guest-agent \
	--disable-werror \
	--disable-download
make
make DESTDIR=$PKG install
