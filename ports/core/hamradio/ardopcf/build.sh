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

# The makefile assigns CFLAGS (-g -MMD) and LDFLAGS (a link map) outright, so
# the environment's flags never reach the compiler unless passed on the make
# line; -MMD is kept because the makefile includes the dependency files it
# writes. There is no install target.
#
# On Linux the CM108 PTT code keeps a file descriptor in a hid_device pointer
# and hands it to read(), write() and close(); GCC 14 and later reject that
# conversion as an error, and -Wno-int-conversion lets the descriptor ride in
# the pointer as upstream wrote it. The TCP host interface and the serial code
# use u_long without including <sys/types.h>, which glibc's socket and unistd
# headers pull in and musl's do not, so it is included ahead of every file.
make ardopcf CFLAGS="$CFLAGS -MMD -Wno-int-conversion -include sys/types.h" LDFLAGS="$LDFLAGS"
install -Dm755 ardopcf "$PKG/usr/bin/ardopcf"

install -d "$PKG/usr/share/doc/ardopcf"
install -m644 README.md changelog.md docs/*.md "$PKG/usr/share/doc/ardopcf/"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ardopcf.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=ardopcf
GenericName=ARDOP Soundcard Modem
Comment=ARDOP HF modem for Pat and Winlink, with a web status page
Exec=ardopcf
Icon=network-wireless
Terminal=true
Categories=Network;HamRadio;
Keywords=ham;radio;ardop;winlink;pat;hf;modem;tnc;
ENTRY
chmod 644 "$PKG/usr/share/applications/ardopcf.desktop"
