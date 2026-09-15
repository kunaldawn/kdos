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

./configure --prefix=/usr 
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/tmux.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Terminal Multiplexer
GenericName=Terminal Multiplexer
Comment=Detachable terminal sessions
Exec=tmux
Icon=system-run
Terminal=true
Categories=System;TerminalEmulator;
Keywords=terminal;session;detach;split;tmux;
EOF
chmod 644 "$PKG/usr/share/applications/tmux.desktop"
