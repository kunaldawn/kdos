# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE TABLE TYPE, and on this machine the only one: visidata explores a file
# and miller streams it, but a join, a group-by and a reshape want this. It is
# cheap once numpy exists — the same meson-python chain, no Fortran and no
# BLAS.
#
# NO WHEELS EXIST FOR THIS TARGET — python3 here is 3.14 on musl and PyPI's
# manylinux wheels are glibc — so this is a from-source build through
# meson-python.
#
# NO VENDOR BUNDLE and --no-build-isolation: meson-python, meson, ninja and
# Cython are all PORTS, so the isolated environment pip would otherwise build
# has nothing to add and everything to fetch. Vendoring instead means pip
# resolving the backend's own chain, which ends at the PyPI `ninja` wrapper
# whose sdist compiles CMake from source.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
