#!/bin/bash


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
