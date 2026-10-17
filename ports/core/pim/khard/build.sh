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
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	vobject .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
(cd doc/source && PYTHONPATH=$_site sphinx-build -b man -q \
	-D extensions=sphinx.ext.autodoc,sphinx.ext.autosectionlabel,sphinx.ext.todo,sphinxarg.ext \
	-d "$SRC_ROOT/khard-doctrees" . "$SRC_ROOT/khard-man")
install -Dm644 "$SRC_ROOT"/khard-man/*.1 -t "$PKG/usr/share/man/man1"
install -Dm644 "$SRC_ROOT"/khard-man/*.5 -t "$PKG/usr/share/man/man5"
