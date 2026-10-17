# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p "$SRC_ROOT/vendor"
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C "$SRC_ROOT/vendor"
pip3 install --ignore-installed --no-deps --no-index --find-links="$SRC_ROOT/vendor" --no-build-isolation \
	--root=$PKG --prefix=/usr \
	pint flexcache flexparser .
