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

export CFLAGS="${CFLAGS/-O2/-O3}"
DLLDFLAGS="$LDFLAGS" ./Configure --prefix=/usr --with-gmp --with-readline --graphic=svg --mt=pthread
make gp
make DESTDIR=$PKG RUNPTH_FINAL= install-doctex install-nodata install-data

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/pari-gp.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=PARI/GP
GenericName=Number Theory Calculator
Comment=Exact arithmetic and number theory at a prompt
Exec=gp
Icon=system-run
Terminal=true
Categories=Education;Science;Math;
Keywords=math;number;theory;prime;factor;pari;gp;calculator;
EOF
chmod 644 "$PKG/usr/share/applications/pari-gp.desktop"
