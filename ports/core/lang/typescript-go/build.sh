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
# noembed is the npm package's build: tsc reads the lib.*.d.ts files from the
# directory of its own resolved path rather than from a copy compiled in, so
# they are installed beside it, and a caller that ships its own edited set
# (Chromium patches lib.dom.d.ts) places a copy of the binary beside those.
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
