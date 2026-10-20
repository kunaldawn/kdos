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

export HELIX_DEFAULT_RUNTIME=/usr/lib/helix/runtime
export HELIX_DISABLE_AUTO_GRAMMAR_BUILD=1
export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline
CARGO_MANIFEST_DIR="$PWD/helix-term" target/release/hx --grammar build
test -f runtime/grammars/rust.so
install -Dm755 target/release/hx $PKG/usr/bin/hx
install -d $PKG/usr/lib/helix
cp -r runtime $PKG/usr/lib/helix/
rm -rf $PKG/usr/lib/helix/runtime/grammars/sources

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/hx.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Editor (helix)
GenericName=Text Editor
Comment=Edit text with helix
Exec=hx %F
Icon=text-x-generic
Terminal=true
Categories=Utility;TextEditor;Development;
Keywords=editor;text;code;helix;hx;
EOF
chmod 644 "$PKG/usr/share/applications/hx.desktop"
