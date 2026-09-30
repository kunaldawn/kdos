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

# --enable-sixel: tmux keeps a sixel image as part of the pane and redraws it
# to an outer terminal that reports sixel, so a picture from timg or chafa
# survives a pane switch. --enable-utf8proc: character widths come from
# utf8proc's Unicode tables rather than from musl's wcwidth.
./configure --prefix=/usr \
	--enable-sixel \
	--enable-utf8proc
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
