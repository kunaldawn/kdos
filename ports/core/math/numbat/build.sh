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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export RUSTFLAGS="-C target-feature=-crt-static"

cargo build --release --frozen --offline --bin numbat
install -Dm755 target/release/numbat $PKG/usr/bin/numbat

install -dm755 $PKG/usr/share/numbat
cp -a numbat/modules $PKG/usr/share/numbat/modules

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/numbat.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Calculator
GenericName=Unit Calculator
Comment=Arithmetic that knows its units
Exec=numbat
Icon=system-run
Terminal=true
Categories=Utility;Calculator;
Keywords=calculator;units;convert;maths;numbat;
EOF
chmod 644 "$PKG/usr/share/applications/numbat.desktop"
