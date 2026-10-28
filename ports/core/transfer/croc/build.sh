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

patch -p1 -i "$PORT_SRC/disable-update-check.patch"

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export CGO_ENABLED=0
go build -mod=vendor \
	-tags croc_no_tailcat,ts_omit_webclient,ts_omit_debugeventbus \
	-ldflags "-s -w -X main.Version=$version" -o croc.bin
install -Dm755 croc.bin $PKG/usr/bin/croc-relay
install -Dm644 packaging/croc.1 -t "$PKG/usr/share/man/man1"

install -Dm755 /dev/stdin $PKG/usr/bin/croc <<'SH'
#!/bin/sh
# --local keeps the transfer on this network. `croc-relay` is the same binary
# with upstream's default, for when reaching the public relay is the intent.
exec /usr/bin/croc-relay --local "$@"
SH
