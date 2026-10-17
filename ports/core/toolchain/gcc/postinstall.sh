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

root="${PKG_ROOT:-/}"
rm -rf "$root/usr/lib/gcc/x86_64-kdos-linux-musl" \
	"$root/usr/x86_64-kdos-linux-musl" \
	"$root/usr/include/c++/$version/x86_64-kdos-linux-musl"
rm -f "$root"/usr/bin/x86_64-kdos-linux-musl-* \
	"$root/usr/share/info/gccinstall.info"
