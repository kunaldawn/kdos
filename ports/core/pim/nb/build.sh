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

install -Dm755 nb $PKG/usr/bin/nb
install -Dm644 etc/nb-completion.bash $PKG/usr/share/bash-completion/completions/nb

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/nb.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Notes
GenericName=Note Taking
Comment=Notes, bookmarks and a searchable archive
Exec=nb
Icon=x-office-document
Terminal=true
Categories=Office;Utility;
Keywords=note;bookmark;archive;wiki;nb;
EOF
chmod 644 "$PKG/usr/share/applications/nb.desktop"
