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

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation \
	--root=$PKG --prefix=/usr .

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/toot.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Fediverse
GenericName=Mastodon Client
Comment=Read and post to the Fediverse
Exec=toot tui
Icon=application-rss+xml
Terminal=true
Categories=Network;
Keywords=mastodon;fediverse;activitypub;social;toot;
EOF2
chmod 644 "$PKG/usr/share/applications/toot.desktop"
