#!/bin/bash


tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# USE `croc --local`. The default relay is a host on the internet and croc
# falls back to it SILENTLY whenever the direct path does not come up — so here
# the flag is not a tuning knob, it is the difference between a transfer
# between two laptops on a table and a transfer through somebody else's server.
# The wrapper makes it the default; the plain binary stays reachable as
# croc-relay for anyone who means the other thing.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o croc.bin
install -Dm755 croc.bin $PKG/usr/bin/croc-relay

install -Dm755 /dev/stdin $PKG/usr/bin/croc <<'SH'
#!/bin/sh
# --local keeps the transfer on this network. `croc-relay` is the same binary
# with upstream's default, for when reaching the public relay is the intent.
exec /usr/bin/croc-relay --local "$@"
SH
