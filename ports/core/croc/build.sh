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

# USE `croc --local`. The default relay is a host on the internet and croc
# falls back to it SILENTLY whenever the direct path does not come up — so here
# the flag is not a tuning knob, it is the difference between a transfer
# between two laptops on a table and a transfer through somebody else's server.
# The wrapper makes it the default; the plain binary stays reachable as
# croc-relay for anyone who means the other thing.
#
# croc_no_tailcat is upstream's relay-only build. It leaves out the Tailscale
# transport, which runs over Tailscale's public DERP servers, and `croc ssh`,
# which is built on it and has no --local of its own; with them go the vendored
# Tailscale web client and htmx, which arrive as minified bundles with no
# sources, and `croc ssh` answers "not supported in this build". The ts_omit tags
# keep those bundles out even if a later croc imports that code from elsewhere.
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
