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

# The bundle holds the four modules only Uranium imports: pyclipper (polygon
# offsets), numpy-stl with python-utils (the STL reader) and colorlog. Their
# build backends are ports, so build isolation stays off and nothing is
# fetched.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr pyclipper numpy-stl python-utils colorlog

# Uranium's CMake installs only through Conan-provided paths and its
# translation tooling, so the layout is laid out here, where the framework
# looks for it from a /usr/bin launcher: the UM package in site-packages,
# plugins under /usr/lib/uranium/plugins, resources under /usr/share/uranium. The
# update checker asks ultimaker.com for new releases at every start and is
# left out. Bundled data is English only, so the translation catalogues are
# left out and the interface keeps its source strings.
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
