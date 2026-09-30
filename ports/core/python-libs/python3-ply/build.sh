# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# NO VENDOR BUNDLE: ply is zero-dependency and pure python, so there is nothing
# to vendor. --no-build-isolation for the same reason — the only thing pip's
# isolated environment would fetch is setuptools, an installed port.
#
# The sdist carries no pyproject.toml, so pip drives setuptools' legacy
# setup.py backend; that backend must be present in the build environment,
# which is exactly what disabling isolation assumes.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
