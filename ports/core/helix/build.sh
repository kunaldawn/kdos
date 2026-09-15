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

cd $name-$version
cargo build --release --locked
install -Dm755 target/release/hx $PKG/usr/bin/hx
install -d $PKG/usr/lib/helix
cp -r runtime $PKG/usr/lib/helix/

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
