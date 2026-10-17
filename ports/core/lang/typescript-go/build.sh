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
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CGO_ENABLED=0
go build -mod=vendor -trimpath -tags=noembed \
	-ldflags "-s -w -X github.com/microsoft/typescript-go/internal/core.version=$version" \
	-o tsc ./cmd/tsgo
install -Dm755 tsc "$PKG/usr/lib/typescript-go/tsc"
install -m644 internal/bundled/libs/*.d.ts "$PKG/usr/lib/typescript-go/"
install -d "$PKG/usr/bin"
ln -s ../lib/typescript-go/tsc "$PKG/usr/bin/tsgo"
install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"
