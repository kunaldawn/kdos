# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The extension modules compile against GEOS's C API, located by geos-config
# on PATH. --no-build-isolation reaches Cython, numpy and setuptools, all
# ports; pip's isolated environment would fetch them over a network this
# build lacks.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
