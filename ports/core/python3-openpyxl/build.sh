# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# WHAT visidata OPENS AN xlsx WITH. `visidata/loaders/xlsx.py` asks for openpyxl
# by name and answers "package `openpyxl` not installed" without it — measured,
# and the same command writes the sheet once this is present. `sc-im` needs
# none of this: it reads the format in C through libzip and libxml2.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pyb() { pip3 install --no-index --find-links=vendor --no-build-isolation "$@"; }
pyb flit-core
pyb packaging
pyb setuptools-scm

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr et-xmlfile .
