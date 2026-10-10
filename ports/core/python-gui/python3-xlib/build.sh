# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# setup.py imports pkg_resources only to check that setuptools is newer than
# 30.3, and setuptools no longer ships pkg_resources; the patch drops the check.
patch -p1 -i "$PORT_SRC/no-pkg-resources.patch"

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
