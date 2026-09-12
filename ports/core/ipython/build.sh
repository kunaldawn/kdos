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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
pip3 install --no-deps --no-index --find-links=vendor --root=$PKG --prefix=/usr .

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ipython.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Python Shell
GenericName=Interactive Python
Comment=A Python prompt with completion and history
Exec=ipython
Icon=system-run
Terminal=true
Categories=Development;
Keywords=python;repl;shell;notebook;ipython;
EOF
chmod 644 "$PKG/usr/share/applications/ipython.desktop"
