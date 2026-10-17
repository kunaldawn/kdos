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

_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" expandvars hatch-fancy-pypi-readme
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	aiohappyeyeballs aiohttp aiosignal aiostream \
	frozenlist multidict propcache tenacity yarl .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
(cd docs && PYTHONPATH=$_site sphinx-build -b man -q \
	-d "$SRC_ROOT/vdirsyncer-doctrees" . "$SRC_ROOT/vdirsyncer-man")
install -Dm644 "$SRC_ROOT/vdirsyncer-man/vdirsyncer.1" -t "$PKG/usr/share/man/man1"
