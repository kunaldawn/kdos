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

cargo build --release --frozen --offline
install -Dm755 target/release/dust $PKG/usr/bin/dust

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/dust.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Disk Usage (dust)
GenericName=Disk Usage Analyser
Comment=A tree of what is filling the disk
Exec=dust
Icon=drive-harddisk
Terminal=true
Categories=System;Filesystem;
Keywords=disk;usage;space;du;dust;
EOF
chmod 644 "$PKG/usr/share/applications/dust.desktop"
install -Dm644 man-page/dust.1 "$PKG/usr/share/man/man1/dust.1"
install -Dm644 completions/dust.bash "$PKG/usr/share/bash-completion/completions/dust"
install -Dm644 completions/dust.fish "$PKG/usr/share/fish/vendor_completions.d/dust.fish"
install -Dm644 completions/_dust     "$PKG/usr/share/zsh/site-functions/_dust"
