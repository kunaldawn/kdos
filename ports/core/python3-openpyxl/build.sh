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

# et-xmlfile and openpyxl are plain setuptools builds, and setuptools is a port.
# lxml and Pillow are ports and not in the bundle: openpyxl reads and writes
# through lxml whenever it imports, and needs Pillow for images in a sheet.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr et-xmlfile .
