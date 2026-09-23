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
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o glow
install -Dm755 glow $PKG/usr/bin/glow
HOME="$PWD/.home" XDG_CACHE_HOME="$PWD/.home" XDG_CONFIG_HOME="$PWD/.home" ./glow man | sed 's/\x1b\[[0-9;]*m//g' > glow.1
install -Dm644 glow.1 -t $PKG/usr/share/man/man1
for sh in bash zsh fish; do
	HOME="$PWD/.home" XDG_CACHE_HOME="$PWD/.home" XDG_CONFIG_HOME="$PWD/.home" ./glow completion $sh > glow.$sh
done
install -Dm644 glow.bash $PKG/usr/share/bash-completion/completions/glow
install -Dm644 glow.zsh  $PKG/usr/share/zsh/site-functions/_glow
install -Dm644 glow.fish $PKG/usr/share/fish/vendor_completions.d/glow.fish

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/glow.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Markdown Reader
GenericName=Document Viewer
Comment=Read Markdown, rendered
Exec=glow %F
Icon=x-office-document
Terminal=true
Categories=Office;Viewer;Utility;
Keywords=markdown;md;readme;render;glow;
EOF
chmod 644 "$PKG/usr/share/applications/glow.desktop"
