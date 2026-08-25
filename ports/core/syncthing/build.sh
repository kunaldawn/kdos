#!/bin/bash


tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# LOCAL DISCOVERY ON, EVERYTHING ELSE OFF — and it is a shipped file rather
# than a note somebody has to read. Global announce, relays and NAT traversal
# each reach the internet BY DESIGN; on a distro that builds offline those are
# three ways for a machine to talk to a stranger without being asked.
# localAnnounce is what makes two machines on the same LAN find each other with
# no server anywhere, which is the entire feature.
export CGO_ENABLED=0
go run -mod=vendor build.go -no-upgrade -version "v$version" build syncthing
install -Dm755 syncthing $PKG/usr/bin/syncthing
for s in 1 5 7; do
	for m in man/*.$s; do
		[ -e "$m" ] || continue
		install -Dm644 "$m" $PKG/usr/share/man/man$s/$(basename "$m")
	done
done

install -Dm644 /dev/stdin $PKG/usr/share/kdos/syncthing-offline.xml <<'XML'
<!-- The <options> block KDOS ships. Merge it into the config syncthing writes
     on first run; these keys are what keep a sync between two machines on one
     LAN from reaching anything outside it. -->
<options>
    <localAnnounceEnabled>true</localAnnounceEnabled>
    <globalAnnounceEnabled>false</globalAnnounceEnabled>
    <relaysEnabled>false</relaysEnabled>
    <natEnabled>false</natEnabled>
    <urAccepted>-1</urAccepted>
    <autoUpgradeIntervalH>0</autoUpgradeIntervalH>
</options>
XML
