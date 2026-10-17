# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr \
	--config-settings=setup-args=-Duse-pythran=false \
	--config-settings=setup-args=-Dblas=openblas \
	--config-settings=setup-args=-Dlapack=openblas \
	.
find "$PKG"/usr/lib/python3*/site-packages/scipy -type d -name tests -prune -exec rm -rf {} +
