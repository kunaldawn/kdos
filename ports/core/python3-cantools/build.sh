# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# can-utils IS THE OTHER HALF AND IS A DEPEND. candump puts raw frames on the
# screen; this is what turns them into named signals against a DBC, and
# generates the C the MCU side decodes them with from the same file.
#
# The bundle is the pure-python remainder. pip skips every requirement that an
# installed port already satisfies, so python-can's packaging and
# typing_extensions are depends: without them installed, the bundle's copies
# land in $PKG and collide with those ports' files. matplotlib is a depend for
# `cantools plot`, which imports it only when that subcommand runs. Build
# isolation stays ON and pointed at the bundle, because these build with
# flit-core and setuptools-scm rather than plain setuptools.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
pip3 install --no-index --find-links=vendor --root=$PKG --prefix=/usr .
