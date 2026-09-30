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

# KDOS RUNS ITS OWN dnsmasq AND HAS NO WAY TO ASK IT ANYTHING. There is no dig,
# no drill and no nslookup in this tree, so "is resolution working, and which
# server answered" has been a question with no tool behind it — on a machine
# that is also expected to BE the DNS server for an island network. doggo
# additionally speaks DoH and DoT, which is what makes it possible to check a
# resolver that is not the local one.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.buildVersion=v$version" -o doggo ./cmd/doggo
install -Dm755 doggo $PKG/usr/bin/doggo
./doggo completions bash > doggo.bash
./doggo completions zsh  > _doggo
./doggo completions fish > doggo.fish
install -Dm644 doggo.bash $PKG/usr/share/bash-completion/completions/doggo
install -Dm644 _doggo     $PKG/usr/share/zsh/site-functions/_doggo
install -Dm644 doggo.fish $PKG/usr/share/fish/vendor_completions.d/doggo.fish
