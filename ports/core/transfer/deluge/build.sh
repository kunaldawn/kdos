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

patch -p1 -i "$PORT_SRC/no-pkg_resources.patch"
patch -p1 -i "$PORT_SRC/pyopenssl-26.patch"
patch -p1 -i "$PORT_SRC/release-check-off-by-default.patch"

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" hatch-fancy-pypi-readme incremental rjsmin
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=/usr \
	twisted automat constantly hyperlink incremental zope.interface \
	pyopenssl service-identity pyasn1 pyasn1-modules rencode \
	setproctitle .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"platbase": sys.argv[1], "base": sys.argv[1]}))' "$PKG/usr")
find "$_site/deluge/i18n" -mindepth 1 -maxdepth 1 -type d ! -name 'en*' -exec rm -rf {} +

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/deluge.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Deluge
GenericName=BitTorrent Client
Comment=Download and share files over BitTorrent
Exec=deluge-gtk %U
Icon=deluge
Terminal=false
StartupWMClass=deluge
Categories=Network;FileTransfer;P2P;GTK;
Keywords=bittorrent;torrent;magnet;download;p2p;deluge;
DESKTOP
chmod 644 "$PKG/usr/share/applications/deluge.desktop"
