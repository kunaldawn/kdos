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

patch -p1 -i "$PORT_SRC/no-embedded-gui.patch"

export CGO_ENABLED=0
go build -mod=vendor -tags noselfupdate \
	-ldflags "-s -w -X github.com/rclone/rclone/fs.Version=v$version" -o rclone
install -Dm755 rclone $PKG/usr/bin/rclone
install -Dm644 rclone.1 $PKG/usr/share/man/man1/rclone.1

./rclone completion bash - > rclone.bash
./rclone completion zsh - > _rclone
./rclone completion fish - > rclone.fish
install -Dm644 rclone.bash $PKG/usr/share/bash-completion/completions/rclone
install -Dm644 _rclone $PKG/usr/share/zsh/site-functions/_rclone
install -Dm644 rclone.fish $PKG/usr/share/fish/vendor_completions.d/rclone.fish
