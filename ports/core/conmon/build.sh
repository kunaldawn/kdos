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

# The Makefile links libsystemd for journald logging whenever pkg-config finds
# one, and seccomp notify whenever libseccomp is 2.5 or newer; there is no
# switch that forces the second on, so `depends` holds it. DISABLE_SYSTEMD
# keeps the first off whatever the build root carries.
make PREFIX=/usr DISABLE_SYSTEMD=1
make PREFIX=/usr DISABLE_SYSTEMD=1 DESTDIR=$PKG install install.crio install.podman
