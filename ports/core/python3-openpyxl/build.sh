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
# BEFORE setuptools-scm, which imports it while its own metadata is generated:
# setuptools-scm 10 is split in two and names vcs-versioning in its build-system
# requires. `--no-build-isolation` installs none of those, so the order of these
# lines IS the build environment.
pyb vcs-versioning
pyb setuptools-scm

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr et-xmlfile .
