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

patch -p1 -i "$PORT_SRC/no-update-check.patch"

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" expandvars coherent-licensed dunamai uv-dynamic-versioning \
	"$SRC_ROOT/pbr-$_pbr"
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr spyder-kernels wurlitzer pyls-spyder

_home=/usr/lib/spyder
export SPYDER_QT_BINDING=pyqt6
export SODIUM_INSTALL=system
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	aiohappyeyeballs aiohttp aiosignal frozenlist multidict propcache yarl \
	arrow asyncssh binaryornot chardet cookiecutter diff-match-patch \
	importlib-metadata inflection intervaltree numpydoc pickleshare pygithub \
	pyjwt pylint-venv pynacl python-slugify pyuca qdarkstyle qstylizer \
	qtawesome qtconsole rtree sortedcontainers superqt text-unidecode \
	textdistance three-merge watchdog zipp .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG$_home")
test -x "$PKG$_home/bin/spyder"
test -d "$_site/spyder"

find "$_site/spyder/locale" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +
rm -rf "$PKG$_home/share"

ln -s /usr/bin/python3 "$PKG$_home/bin/python"
printf 'home = /usr/bin\ninclude-system-site-packages = true\n' > "$PKG$_home/pyvenv.cfg"
python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' \
	/usr/lib/python3-sphinx > "$_site/python3-sphinx.pth"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/spyder" <<'KDOS_SH'
#!/bin/sh
QT_API=pyqt6
SPATIALINDEX_C_LIBRARY=/usr/lib/libspatialindex_c.so
export QT_API SPATIALINDEX_C_LIBRARY
exec /usr/lib/spyder/bin/python /usr/lib/spyder/bin/spyder "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/spyder"

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s spyder/images/spyder.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/spyder.png"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/spyder.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Spyder
GenericName=Scientific Python IDE
Comment=Edit, run, debug and explore data in Python
TryExec=spyder
Exec=spyder %F
Icon=spyder
Terminal=false
StartupNotify=true
StartupWMClass=spyder
Categories=Development;Science;IDE;Qt;
Keywords=python;ide;science;data;ipython;variable;explorer;spyder;
DESKTOP
chmod 644 "$PKG/usr/share/applications/spyder.desktop"
