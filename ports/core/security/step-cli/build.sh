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

export CGO_ENABLED=1
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step ./cmd/step
install -Dm755 step $PKG/usr/bin/step
install -Dm644 autocomplete/bash_autocomplete \
	$PKG/usr/share/bash-completion/completions/step
install -Dm644 autocomplete/zsh_autocomplete \
	$PKG/usr/share/zsh/site-functions/_step
