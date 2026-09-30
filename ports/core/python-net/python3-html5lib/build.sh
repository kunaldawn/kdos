# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# setup.py reads its version through ast.Str, which Python 3.14 removed, and
# imports pkg_resources, which setuptools 82 removed; either stops the build
# before a file is installed.
patch -p1 -i "$PORT_SRC/html5lib-python-3.14.patch"
patch -p1 -i "$PORT_SRC/html5lib-setuptools-82.patch"

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
