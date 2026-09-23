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
make GO_LDFLAGS="-s -w" GH_VERSION=$version bin/gh manpages
install -Dm755 bin/gh $PKG/usr/bin/gh
install -Dm644 share/man/man1/*.1 -t $PKG/usr/share/man/man1

for sh in bash zsh fish; do
	HOME="$PWD/.home" GH_CONFIG_DIR="$PWD/.home" bin/gh completion -s $sh > gh.$sh
done
install -Dm644 gh.bash $PKG/usr/share/bash-completion/completions/gh
install -Dm644 gh.zsh  $PKG/usr/share/zsh/site-functions/_gh
install -Dm644 gh.fish $PKG/usr/share/fish/vendor_completions.d/gh.fish
