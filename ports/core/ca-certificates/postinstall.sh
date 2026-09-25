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

# /etc/ssl/cert.pem is the Mozilla bundle plus the local roots, and is written
# here rather than shipped: a package-owned copy would be replaced by every
# upgrade and lose the local roots. The tool is the one just installed, run
# against PKG_ROOT, so an install into another root writes that root's bundle.
root="${PKG_ROOT:-/}"
root="${root%/}"
"$root/usr/bin/update-ca-certificates" --root "${root:-/}"
