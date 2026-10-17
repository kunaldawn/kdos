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


mkdir -p vendor
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1 -C vendor

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr pyclipper numpy-stl python-utils colorlog
rm -f "$PKG/usr/bin/stl" "$PKG/usr/bin/stl2ascii" "$PKG/usr/bin/stl2bin"
rmdir "$PKG/usr/bin"

_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib", vars={"base": "/usr"}))')
install -d "$PKG$_site" "$PKG/usr/lib/uranium/plugins" "$PKG/usr/share/uranium"
cp -r UM "$PKG$_site/"
for p in plugins/*; do
	[ "${p##*/}" = UpdateChecker ] && continue
	cp -r "$p" "$PKG/usr/lib/uranium/plugins/"
done
cp -r resources "$PKG/usr/share/uranium/"
rm -rf "$PKG/usr/share/uranium/resources/i18n"
find "$PKG$_site/UM" "$PKG/usr/lib/uranium" -depth -type d \
	\( -name tests -o -name __pycache__ \) -exec rm -rf {} +
find "$PKG$_site/UM" "$PKG/usr/lib/uranium" "$PKG/usr/share/uranium" -type f -exec chmod 644 {} +
python3 -m compileall -q -s "$PKG" -p / "$PKG$_site/UM" "$PKG/usr/lib/uranium"

test -f "$PKG$_site/UM/Qt/qml/UM/qmldir"
test -f "$PKG/usr/lib/uranium/plugins/FileHandlers/STLReader/plugin.json"
ls "$PKG$_site"/pyclipper* "$PKG$_site"/stl >/dev/null
