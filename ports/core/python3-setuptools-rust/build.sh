# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# NO VENDOR BUNDLE and --no-build-isolation: this is pure python with a
# setuptools backend, and setuptools is an installed port. An isolated build
# environment would fetch it from PyPI, which is a build with no network
# failing at a download rather than at a compiler.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
