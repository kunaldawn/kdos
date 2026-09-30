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

# The release is the database itself, XML in the layout libosinfo reads, so
# installing is a copy into the system location. Validating against the
# schema it carries first means a malformed file fails here rather than
# making libosinfo skip that operating system at run time.
osinfo-db-validate --dir "$SRC"
install -d "$PKG/usr/share/osinfo"
cp -R . "$PKG/usr/share/osinfo/"
find "$PKG/usr/share/osinfo" -type d -exec chmod 755 {} +
find "$PKG/usr/share/osinfo" -type f -exec chmod 644 {} +
