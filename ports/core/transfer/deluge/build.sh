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
# The daemon asks deluge-torrent.org for a newer release at every start;
# the patch makes the check opt-in.
patch -p1 -i "$PORT_SRC/release-check-off-by-default.patch"

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# THE CLOSURE IS IN THE BUNDLE, except where a module is already a port.
# chardet and ifaddr are optional imports and are left out: ifaddr's paths in
# site-packages belong to cura's bundle, and without chardet a tracker page in
# an undeclared character set is decoded as UTF-8 or Latin-1.
#
# BUILD ISOLATION IS OFF, so each sdist builds with its backend installed.
# hatch-fancy-pypi-readme and incremental are Twisted's build plugins, and
# rjsmin rebuilds the web UI's minified script from its sources; they go
# into the build root, not into $PKG. --break-system-packages because
# python3 marks its site-packages as kpkg's (PEP 668).
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--break-system-packages hatch-fancy-pypi-readme incremental rjsmin

# --ignore-installed: incremental is also in the build root now, and pip
# would otherwise count it as present and leave it out of $PKG.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=/usr \
	twisted automat constantly hyperlink incremental zope.interface \
	pyopenssl service-identity pyasn1 pyasn1-modules rencode \
	setproctitle distro .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"platbase": sys.argv[1], "base": sys.argv[1]}))' "$PKG/usr")
find "$_site/deluge/i18n" -mindepth 1 -maxdepth 1 -type d ! -name 'en*' -exec rm -rf {} +

# set_prgname('deluge') makes the window's app_id deluge. qBittorrent is the
# torrent and magnet handler, so this entry claims no MIME type.
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
