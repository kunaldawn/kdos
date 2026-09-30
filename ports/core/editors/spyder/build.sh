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

# The update manager queries GitHub and PyPI at every start and has no build
# switch; the patch makes its default off, so an offline machine is never
# contacted. The check stays available in Preferences and the Help menu.
patch -p1 -i "$PORT_SRC/no-update-check.patch"

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# Four build backends in the bundle are not ports. They are installed into a
# directory of their own on PYTHONPATH for this build only, so nothing outside
# the package is written.
_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" expandvars coherent-licensed dunamai uv-dynamic-versioning
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

# THE KERNEL SIDE GOES IN site-packages. Spyder removes PYTHONPATH from the
# environment of every kernel and language server it starts, so what those
# processes import must be importable by plain python3: spyder-kernels and
# wurlitzer (the console's kernel) and pyls-spyder (the outline plugin of the
# python3-lsp-server port). Spyder is their only user.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr spyder-kernels wurlitzer pyls-spyder

# THE WINDOW'S OWN MODULES GO UNDER /usr/lib/spyder, beside python3-sphinx's
# /usr/lib/python3-sphinx: several are also in other ports' bundles (aiohttp
# in vdirsyncer's, arrow in jupyterlab's), and two packages owning one path in
# site-packages is a conflict. The launcher below puts both directories on
# PYTHONPATH; the help pane renders docstrings with that Sphinx. The binding
# is PyQt6 (the metadata's default is PyQt5, which is not installed). PyNaCl
# links the libsodium port rather than its bundled copy.
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

# Bundled data is English only: the translation directories go, and the
# locale directory stays, because Spyder lists it to offer languages.
find "$_site/spyder/locale" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +
# setup.py's data files land under the private prefix; the entry and icon
# are written below in their system places.
rm -rf "$PKG$_home/share"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/spyder" <<'KDOS_SH'
#!/bin/sh
home=/usr/lib/spyder
site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$home") || exit 1
sphinx=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' /usr/lib/python3-sphinx) || exit 1
PYTHONPATH=$site:$sphinx${PYTHONPATH:+:$PYTHONPATH}
QT_API=pyqt6
SPATIALINDEX_C_LIBRARY=/usr/lib/libspatialindex_c.so
export PYTHONPATH QT_API SPATIALINDEX_C_LIBRARY
exec "$home/bin/spyder" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/spyder"

# THE MENU ICON: rasterised from the SVG the program itself uses.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s spyder/images/spyder.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/spyder.png"
done

# The program sets the desktop file name, and so the Wayland app_id, to
# spyder. text/x-python is every editor's type, so the entry claims none.
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
