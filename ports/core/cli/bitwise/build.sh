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

autoreconf -fi
./configure --prefix=/usr
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/bitwise.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Bit Calculator
GenericName=Bitwise Calculator
Comment=Bases, bit fields and bitwise arithmetic
Exec=bitwise
Icon=system-run
Terminal=true
Categories=Utility;Calculator;
Keywords=calculator;hex;binary;bits;bitwise;
EOF
chmod 644 "$PKG/usr/share/applications/bitwise.desktop"
