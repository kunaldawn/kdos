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


# A SINGLE SCRIPT AND NOT AN ARCHIVE, so it is not unpacked — it stays in the
# port directory and is installed from there.
#
# openconnect EXECUTES IT ON EVERY CONNECT to set routes, DNS and the tunnel
# address; a tunnel that comes up without it is a link with no route on it,
# which reads as a VPN that connected and does nothing.
install -Dm755 "$PORT_SRC/vpnc-script" "$PKG/usr/share/vpnc-scripts/vpnc-script"
