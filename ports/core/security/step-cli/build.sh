# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/command-name.patch"

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export CGO_ENABLED=1
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step-cli ./cmd/step
install -Dm755 step-cli $PKG/usr/bin/step-cli
install -Dm644 autocomplete/bash_autocomplete \
	$PKG/usr/share/bash-completion/completions/step-cli
install -Dm644 autocomplete/zsh_autocomplete \
	$PKG/usr/share/zsh/site-functions/_step-cli
