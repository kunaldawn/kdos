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

./configure --prefix=/usr \
	--enable-sixel \
	--enable-utf8proc
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/tmux.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=tmux
GenericName=Terminal Multiplexer
Comment=Detachable terminal sessions
Exec=tmux
Icon=utilities-terminal
Terminal=true
Categories=System;ConsoleOnly;
Keywords=terminal;session;detach;split;tmux;
EOF
chmod 644 "$PKG/usr/share/applications/tmux.desktop"
