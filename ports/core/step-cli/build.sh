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

# THE OTHER HALF OF step-ca. The server runs from a ca.json it cannot write:
# `step ca init` creates the root and intermediate keys, the certificates and
# that configuration, and every client-side verb — bootstrapping trust in a
# root, requesting, renewing and revoking a certificate, adding a provisioner —
# is this program talking to the CA's API. It also stands alone as a
# certificate and JWT toolkit: `step certificate inspect` reads any PEM.
#
# A hardware-held CA key is step-ca's business, through its own kms block;
# this program links no card library. BuildTime is left unset: a wall-clock
# stamp would make every build a different binary.
export CGO_ENABLED=1
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step ./cmd/step
install -Dm755 step $PKG/usr/bin/step
install -Dm644 autocomplete/bash_autocomplete \
	$PKG/usr/share/bash-completion/completions/step
install -Dm644 autocomplete/zsh_autocomplete \
	$PKG/usr/share/zsh/site-functions/_step
