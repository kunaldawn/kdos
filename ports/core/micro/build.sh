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

export CGO_ENABLED=0
make build
install -Dm755 micro $PKG/usr/bin/micro

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/micro.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Editor (micro)
GenericName=Text Editor
Comment=Edit text with micro
Exec=micro %F
Icon=text-x-generic
Terminal=true
Categories=Utility;TextEditor;
Keywords=editor;text;micro;
EOF
chmod 644 "$PKG/usr/share/applications/micro.desktop"
