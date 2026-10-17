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

export CGO_ENABLED=0
export EXTRA_LDFLAGS=-s
go run -mod=vendor build.go -no-upgrade -version "v$version" build syncthing
install -Dm755 syncthing $PKG/usr/bin/syncthing
for s in 1 5 7; do
	for m in man/syncthing*.$s; do
		[ -e "$m" ] || continue
		install -Dm644 "$m" $PKG/usr/share/man/man$s/$(basename "$m")
	done
done

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
