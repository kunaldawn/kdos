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
