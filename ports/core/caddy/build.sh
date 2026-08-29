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

# CADDY'S DEFAULT CONFIG TRIES LET'S ENCRYPT AND FAILS ON EVERY START, which on
# an offline machine is not a warning but a server that never comes up. The
# shipped Caddyfile therefore uses `tls internal` — Caddy's own local CA, which
# needs nothing outside this machine — so `https://` works on the LAN from the
# first boot and the browser's warning is answered by trusting one root rather
# than by turning TLS off.
#
# What it serves: kiwix's ZIM front end, a local git forge, anything else on
# the island network. It is one static binary with no module system to
# assemble, which is why it is a port and nginx is not.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w" -o caddy ./cmd/caddy
install -Dm755 caddy $PKG/usr/bin/caddy

install -Dm644 /dev/stdin $PKG/etc/caddy/Caddyfile <<'CFG'
# `tls internal` is the whole point of this file. Caddy's default is to ask
# Let's Encrypt for a certificate, which on a machine with no route to the
# internet fails at every start — so the server that was supposed to serve the
# offline corpus is the one thing that does not come up. `internal` issues from
# Caddy's own local CA; trust its root once and every host below is https.
{
	auto_https disable_redirects
	local_certs
}

# The corpus, served to any browser on the LAN with no client install.
# kiwix-serve is expected on 8080; change the port, not the shape.
:8443 {
	tls internal
	reverse_proxy localhost:8080
}
CFG
