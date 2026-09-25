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
	for m in man/syncthing*.$s; do
		[ -e "$m" ] || continue
		install -Dm644 "$m" $PKG/usr/share/man/man$s/$(basename "$m")
	done
done

# A WHOLE CONFIGURATION WITH ONLY <options> IN IT, because syncthing reads a
# config.xml over its own defaults: every key not named here keeps upstream's
# value, the device's own entry and keys are made on first start, and no
# default folder is created. version is the schema this release writes, so no
# migration runs over it.
install -Dm644 /dev/stdin $PKG/usr/share/kdos/syncthing-offline.xml <<'XML'
<!-- The configuration kdos-syncthing gives a user who has none. These keys
     are what keep a sync between two machines on one LAN from reaching
     anything outside it. -->
<configuration version="52">
    <options>
        <localAnnounceEnabled>true</localAnnounceEnabled>
        <globalAnnounceEnabled>false</globalAnnounceEnabled>
        <relaysEnabled>false</relaysEnabled>
        <natEnabled>false</natEnabled>
        <urAccepted>-1</urAccepted>
        <autoUpgradeIntervalH>0</autoUpgradeIntervalH>
    </options>
</configuration>
XML

# The launcher seeds that file where syncthing will look for its config — the
# same search syncthing makes — and only when nothing is there, so a
# configuration somebody already has is never touched. Then it is syncthing.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/kdos-syncthing" <<'KDOS_SH'
#!/bin/sh
if [ -n "$STHOMEDIR" ]; then
	dir=$STHOMEDIR
elif [ -n "$STCONFDIR" ]; then
	dir=$STCONFDIR
elif [ -n "$XDG_CONFIG_HOME" ] && [ -f "$XDG_CONFIG_HOME/syncthing/config.xml" ]; then
	dir=$XDG_CONFIG_HOME/syncthing
elif [ -f "$HOME/.config/syncthing/config.xml" ]; then
	dir=$HOME/.config/syncthing
else
	case "$XDG_STATE_HOME" in
	/*) dir=$XDG_STATE_HOME/syncthing ;;
	*) dir=$HOME/.local/state/syncthing ;;
	esac
fi
if [ ! -e "$dir/config.xml" ]; then
	mkdir -p "$dir" && chmod 700 "$dir" &&
		install -m600 /usr/share/kdos/syncthing-offline.xml "$dir/config.xml"
fi
exec syncthing serve "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/kdos-syncthing"

# The web interface is on 127.0.0.1:8384, and `serve` opens it in the browser
# once it is listening. Closing the terminal stops the sync.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/syncthing.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=File Sync
GenericName=Syncthing
Comment=Keep folders the same on the machines on this network
Exec=kdos-syncthing
Icon=folder-syncthing
Terminal=true
Categories=Network;FileTransfer;
Keywords=sync;syncthing;folder;share;backup;
EOF
chmod 644 "$PKG/usr/share/applications/syncthing.desktop"
