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

# UNITS IN THE TYPE SYSTEM IS THE WHOLE FEATURE. `bc` will happily add a
# voltage to a resistance; this refuses, and converts when the conversion is
# meaningful. On a bench where the other tools are a scope, an SDR and a
# soldering iron, the arithmetic mistake that actually happens is a unit one.
cargo build --release --frozen --offline --bin numbat
install -Dm755 target/release/numbat $PKG/usr/bin/numbat

# The unit and constant definitions are DATA it loads at run time, not
# compiled in — without them numbat starts and knows no units at all, which
# looks like a broken install rather than a missing directory.
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
